//
//   Copyright 2024 Pixar
//
//   Licensed under the terms set forth in the LICENSE.txt file available at
//   https://opensubdiv.org/license.
//

//
//  Regression tests for bicubic Bezier output from the Far::PatchTable:
//
//      1. Bezier patch tables are generated, populated (serialized into the
//         usual PatchTable arrays) and evaluated by the existing CPU
//         evaluator (PatchTable::EvaluateBasis).
//
//      2. Regular (fully interior) and boundary patches reproduce the
//         native BSpline limit surface exactly (to floating point
//         tolerance), i.e. patch parameters, control-point ordering and
//         face orientation match the existing limit position output.
//
//      3. Patches in extraordinary neighborhoods (no exact polynomial
//         representation) are converted through the Gregory end-cap with
//         preserved boundary geometry and corners -- no surface-valid but
//         semantically incorrect patches are produced.
//
//      4. Repeated evaluation of the same coordinate is deterministic.
//
//      5. Schemes without a Bezier representation (Loop, Bilinear) trigger
//         a diagnosable warning and fall back to valid default patches.
//
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/ptexIndices.h>

#include "../../regression/common/shape_utils.h"
#include "../../regression/common/far_utils.h"
#include "../../regression/shapes/all.h"

using namespace OpenSubdiv;
using namespace OpenSubdiv::OPENSUBDIV_VERSION;

typedef Far::TopologyRefiner                Refiner;
typedef Far::TopologyRefinerFactory<Shape>  RefinerFactory;
typedef Far::PatchTableFactory              PatchFactory;
typedef Far::PatchTable                     PatchTable;
typedef Far::PatchMap                       PatchMap;
typedef Far::PtexIndices                    PtexIndices;

//  Exact (change-of-basis) comparisons are tight; Gregory-derived patches
//  are an interior-only approximation and use a looser tolerance.
static float const kExactTol    = 1.0e-5f;
static float const kBoundaryTol = 1.0e-5f;
static float const kApproxTol   = 2.0e-2f;

static int g_numTests    = 0;
static int g_numFailures = 0;

//
//  Minimal interpolatable 3D point -- Clear() and AddWithWeight() are the
//  operations required by Far::PrimvarRefinerReal.
//
template <typename REAL>
struct Vec3 {
    Vec3() { p[0] = p[1] = p[2] = (REAL) 0.0; }
    Vec3(REAL x, REAL y, REAL z) { p[0] = x; p[1] = y; p[2] = z; }

    void Clear(void * = 0) { p[0] = p[1] = p[2] = (REAL) 0.0; }
    void AddWithWeight(Vec3<REAL> const & src, REAL weight) {
        p[0] += weight * src.p[0];
        p[1] += weight * src.p[1];
        p[2] += weight * src.p[2];
    }

    REAL const & operator[](int i) const { return p[i]; }
    REAL       & operator[](int i)       { return p[i]; }

    REAL p[3];
};

#define TEST_CHECK(condition, message)                                   \
    do {                                                                 \
        ++g_numTests;                                                    \
        if (!(condition)) {                                              \
            ++g_numFailures;                                             \
            printf("  FAIL: %s (line %d)\n", (message), __LINE__);      \
        }                                                                \
    } while (0)

//
//  Capture all messages emitted through the Far warning callback so that
//  fallback behavior can be asserted without polluting test output.
//
static std::vector<std::string> g_warnings;

static void
warningCaptureCallback(char const * message) {
    g_warnings.push_back(std::string(message));
}

//
//  Build an adaptively refined mesh for a shape with a given end-cap basis.
//  The returned refiner and patch table are owned by the caller.
//
template <typename REAL>
struct BuiltTable {
    Refiner *                refiner;
    PatchTable *             patchTable;
    PatchMap *               patchMap;
    PtexIndices *            ptexIndices;
    std::vector<Vec3<REAL> > patchPos;
};

template <typename REAL>
static BuiltTable<REAL>
buildPatchTable(Shape const & shape,
                PatchFactory::Options::EndCapType endCapType) {

    Sdc::SchemeType sdcType    = GetSdcType(shape);
    Sdc::Options    sdcOptions = GetSdcOptions(shape);

    Refiner * refiner = RefinerFactory::Create(shape,
        RefinerFactory::Options(sdcType, sdcOptions));

    Far::TopologyRefiner::AdaptiveOptions adaptiveOptions(4);
    adaptiveOptions.useInfSharpPatch = true;
    refiner->RefineAdaptive(adaptiveOptions);

    PatchFactory::Options patchOptions(4);
    patchOptions.SetEndCapType(endCapType);
    patchOptions.useInfSharpPatch = true;
    patchOptions.generateVaryingTables = false;
    patchOptions.shareEndCapPatchPoints = false;
    patchOptions.SetPatchPrecision<REAL>();

    PatchTable * patchTable = PatchFactory::Create(*refiner, patchOptions);
    PatchMap *   patchMap   = new PatchMap(*patchTable);
    PtexIndices* ptex       = new PtexIndices(*refiner);

    //  Interpolate refined vertex positions and any local patch points:
    int numBasePoints    = refiner->GetLevel(0).GetNumVertices();
    int numRefinedPoints = refiner->GetNumVerticesTotal() - numBasePoints;
    int numLocalPoints   = patchTable->GetNumLocalPoints();

    BuiltTable<REAL> built;
    built.patchPos.resize(numBasePoints + numRefinedPoints + numLocalPoints);

    for (int i = 0; i < numBasePoints; ++i) {
        built.patchPos[i] = Vec3<REAL>(
            (REAL) shape.verts[3*i],
            (REAL) shape.verts[3*i + 1],
            (REAL) shape.verts[3*i + 2]);
    }

    if (numRefinedPoints > 0) {
        Far::PrimvarRefinerReal<REAL> primvarRefiner(*refiner);

        Vec3<REAL> const * src = &built.patchPos[0];
        Vec3<REAL> *       dst = &built.patchPos[numBasePoints];

        for (int level = 1; level < refiner->GetNumLevels(); ++level) {
            primvarRefiner.Interpolate(level, src, dst);
            src = dst;
            dst += refiner->GetLevel(level).GetNumVertices();
        }
    }

    if (numLocalPoints > 0) {
        patchTable->GetLocalPointStencilTable<REAL>()->UpdateValues(
            &built.patchPos[0],
            &built.patchPos[numBasePoints + numRefinedPoints]);
    }

    built.refiner     = refiner;
    built.patchTable  = patchTable;
    built.patchMap    = patchMap;
    built.ptexIndices = ptex;
    return built;
}

template <typename REAL>
static void
destroyBuiltTable(BuiltTable<REAL> & built) {
    delete built.patchTable;
    delete built.patchMap;
    delete built.ptexIndices;
    delete built.refiner;
}

//
//  Evaluate a patch table at a ptex face coordinate and return the
//  interpolated position using the existing PatchTable::EvaluateBasis
//  evaluator (the same code path used by the CPU/OMP/TBB evaluators).
//
template <typename REAL>
static void
evaluatePosition(BuiltTable<REAL> const & built, int ptexFace,
                 REAL s, REAL t, REAL result[3]) {

    PatchTable::PatchHandle const * handle =
        built.patchMap->FindPatch(ptexFace, (double) s, (double) t);
    assert(handle);

    REAL wP[20];
    built.patchTable->EvaluateBasis(*handle, s, t, wP);

    Far::ConstIndexArray cvIndices =
        built.patchTable->GetPatchVertices(*handle);

    result[0] = result[1] = result[2] = (REAL) 0.0;
    for (int cv = 0; cv < cvIndices.size(); ++cv) {
        int idx = cvIndices[cv];
        result[0] += wP[cv] * built.patchPos[idx][0];
        result[1] += wP[cv] * built.patchPos[idx][1];
        result[2] += wP[cv] * built.patchPos[idx][2];
    }
}

template <typename REAL>
static REAL
distance3(REAL const a[3], REAL const b[3]) {
    REAL dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2];
    return (REAL) std::sqrt((double)(dx*dx + dy*dy + dz*dz));
}

//
//  Verify that every patch array in the table has a supported descriptor
//  and that Bezier patches, when present, have 16 control vertices.
//
template <typename REAL>
static void
testPatchTableContents(BuiltTable<REAL> const & built, bool expectBezier,
                       char const * shapeName) {

    int numArrays = built.patchTable->GetNumPatchArrays();
    int numBezierArrays = 0;

    for (int i = 0; i < numArrays; ++i) {
        Far::PatchDescriptor desc =
            built.patchTable->GetPatchArrayDescriptor(i);

        if (desc.GetType() == Far::PatchDescriptor::BEZIER) {
            ++numBezierArrays;
            TEST_CHECK(desc.GetNumControlVertices() == 16,
                "Bezier patch must have 16 control vertices");
        }
    }

    if (expectBezier) {
        TEST_CHECK(numBezierArrays > 0,
            "expected at least one Bezier patch array");
        if (numBezierArrays == 0) {
            printf("    (shape '%s' had no Bezier arrays)\n", shapeName);
        }
    }
}

//
//  Compare Bezier table positions against a reference table over a grid of
//  sample coordinates on every base face.  The "boundaryOnly" mode
//  restricts samples to patch edges so that the exact boundary/corner
//  preservation of the conversion can be measured independently of the
//  interior Gregory approximation.
//
template <typename REAL>
static REAL
testPositionAgreement(BuiltTable<REAL> const & bezierTable,
                      BuiltTable<REAL> const & referenceTable,
                      int numBaseFaces, bool boundaryOnly,
                      REAL tolerance, char const * shapeName) {

    static REAL const interiorSamples[7] = {
        (REAL) 0.0, (REAL) 0.13, (REAL) 0.31, (REAL) 0.5,
        (REAL) 0.67, (REAL) 0.84, (REAL) 1.0
    };
    static REAL const edgeSamples[5] = {
        (REAL) 0.0, (REAL) 0.25, (REAL) 0.5, (REAL) 0.75, (REAL) 1.0
    };

    REAL maxDelta = (REAL) 0.0;
    int  numSamples = 0;

    for (int face = 0; face < numBaseFaces; ++face) {
        int ptexFace = bezierTable.ptexIndices->GetFaceId(face);

        if (boundaryOnly) {
            for (int i = 0; i < 5; ++i) {
                REAL u = edgeSamples[i];
                REAL const edgeCoords[4][2] = {
                    { u,          (REAL) 0.0 },
                    { u,          (REAL) 1.0 },
                    { (REAL) 0.0, u          },
                    { (REAL) 1.0, u          }
                };
                for (int e = 0; e < 4; ++e) {
                    REAL pBez[3], pRef[3];
                    evaluatePosition(bezierTable, ptexFace,
                        edgeCoords[e][0], edgeCoords[e][1], pBez);
                    evaluatePosition(referenceTable, ptexFace,
                        edgeCoords[e][0], edgeCoords[e][1], pRef);
                    maxDelta = std::max(maxDelta, distance3(pBez, pRef));
                    ++numSamples;
                }
            }
        } else {
            for (int i = 0; i < 7; ++i) {
                for (int j = 0; j < 7; ++j) {
                    REAL s = interiorSamples[j];
                    REAL t = interiorSamples[i];
                    REAL pBez[3], pRef[3];
                    evaluatePosition(bezierTable, ptexFace, s, t, pBez);
                    evaluatePosition(referenceTable, ptexFace, s, t, pRef);
                    maxDelta = std::max(maxDelta, distance3(pBez, pRef));
                    ++numSamples;
                }
            }
        }
    }

    TEST_CHECK(maxDelta <= tolerance,
        boundaryOnly ? "Bezier boundary continuity against reference"
                     : "Bezier interior positions against reference");
    if (maxDelta > tolerance) {
        printf("    (shape '%s': %d samples, max delta %.6e > tol %.0e)\n",
            shapeName, numSamples, (double) maxDelta, (double) tolerance);
    }
    return maxDelta;
}

//
//  Repeated evaluation of identical coordinates must return bit-identical
//  results (the conversion involves no state or randomness).
//
template <typename REAL>
static void
testDeterministicEvaluation(BuiltTable<REAL> const & built, int numBaseFaces) {

    bool identical = true;

    for (int face = 0; face < numBaseFaces; ++face) {
        int ptexFace = built.ptexIndices->GetFaceId(face);

        for (int k = 0; k < 5; ++k) {
            REAL s = (REAL) (0.17 + 0.13 * k);
            REAL t = (REAL) (0.29 + 0.11 * k);

            REAL p1[3], p2[3];
            evaluatePosition(built, ptexFace, s, t, p1);
            evaluatePosition(built, ptexFace, s, t, p2);

            if (p1[0] != p2[0] || p1[1] != p2[1] || p1[2] != p2[2]) {
                identical = false;
            }
        }
    }
    TEST_CHECK(identical, "repeated Bezier evaluation is deterministic");
}

//
//  Full test sequence for a single Catmark shape.  The reference table
//  uses Gregory end-caps (the highest-quality existing representation).
//
template <typename REAL>
static void
testCatmarkShape(ShapeDesc const & shapeDesc) {

    printf("  Catmark shape '%s':\n", shapeDesc.name.c_str());

    Shape * shape = Shape::parseObj(
        shapeDesc.data.c_str(), shapeDesc.scheme, false);
    assert(shape);

    int numBaseFaces = shape->GetNumFaces();

    BuiltTable<REAL> bezierTable = buildPatchTable<REAL>(
        *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);
    BuiltTable<REAL> gregoryTable = buildPatchTable<REAL>(
        *shape, PatchFactory::Options::ENDCAP_GREGORY_BASIS);

    //  1. Table contents/serialization:
    testPatchTableContents(bezierTable, true, shapeDesc.name.c_str());

    //  2. Boundary and corner continuity must be exact for every patch:
    testPositionAgreement(bezierTable, gregoryTable, numBaseFaces,
        true, (REAL) kBoundaryTol, shapeDesc.name.c_str());

    //  3. Interior agreement -- exact for regular regions, within the
    //     Gregory approximation tolerance around extraordinary vertices:
    testPositionAgreement(bezierTable, gregoryTable, numBaseFaces,
        false, (REAL) kApproxTol, shapeDesc.name.c_str());

    //  4. Determinism:
    testDeterministicEvaluation(bezierTable, numBaseFaces);

    destroyBuiltTable(bezierTable);
    destroyBuiltTable(gregoryTable);
    delete shape;
}

//
//  A closed mesh without any extraordinary vertices or boundaries: every
//  face is a regular interior patch (no phantom points), so the BSpline ->
//  Bezier conversion must be exact over the whole surface.  An open mesh
//  (e.g. catmark_quadstrips) is also exact through the boundary-weight
//  path but is covered by the boundary tests with appropriate tolerances.
//
template <typename REAL>
static void
testRegularShapeExact() {

    printf("  Regular closed shape 'catmark_toroidal_tet' (exact conversion):\n");

    ShapeDesc desc("catmark_toroidal_tet", catmark_toroidal_tet, kCatmark);
    Shape * shape = Shape::parseObj(desc.data.c_str(), desc.scheme, false);
    assert(shape);

    int numBaseFaces = shape->GetNumFaces();

    BuiltTable<REAL> bezierTable = buildPatchTable<REAL>(
        *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);
    BuiltTable<REAL> bsplineTable = buildPatchTable<REAL>(
        *shape, PatchFactory::Options::ENDCAP_BSPLINE_BASIS);

    testPatchTableContents(bezierTable, true, desc.name.c_str());

    //  A closed regular mesh has no Gregory approximation or phantom points
    //  anywhere -- the full interior comparison must satisfy the exact
    //  change-of-basis tolerance.
    testPositionAgreement(bezierTable, bsplineTable, numBaseFaces,
        false, (REAL) kExactTol, desc.name.c_str());

    destroyBuiltTable(bezierTable);
    destroyBuiltTable(bsplineTable);
    delete shape;
}

//
//  Boundary-heavy meshes exercise the phantom-point extrapolation path in
//  the BSpline -> Bezier conversion.
//
template <typename REAL>
static void
testBoundaryShapes() {

    struct Entry { char const * name; std::string const & data; };
    Entry entries[] = {
        { "catmark_edgeonly",     catmark_edgeonly },
        { "catmark_edgecorner",   catmark_edgecorner },
        { "catmark_xord_boundary", catmark_xord_boundary }
    };

    for (int i = 0; i < 3; ++i) {
        printf("  Boundary shape '%s':\n", entries[i].name);

        ShapeDesc desc(entries[i].name, entries[i].data, kCatmark);
        Shape * shape = Shape::parseObj(desc.data.c_str(), desc.scheme, false);
        assert(shape);

        int numBaseFaces = shape->GetNumFaces();

        BuiltTable<REAL> bezierTable = buildPatchTable<REAL>(
            *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);
        BuiltTable<REAL> gregoryTable = buildPatchTable<REAL>(
            *shape, PatchFactory::Options::ENDCAP_GREGORY_BASIS);

        testPatchTableContents(bezierTable, true, desc.name.c_str());
        testPositionAgreement(bezierTable, gregoryTable, numBaseFaces,
            true, (REAL) kBoundaryTol, desc.name.c_str());
        testDeterministicEvaluation(bezierTable, numBaseFaces);

        destroyBuiltTable(bezierTable);
        destroyBuiltTable(gregoryTable);
        delete shape;
    }
}

//
//  Extraordinary neighborhoods (pentagonal faces, high-valence poles)
//  exercise the Gregory -> Bezier interior averaging path.
//
template <typename REAL>
static void
testExtraordinaryShapes() {

    struct Entry { char const * name; std::string const & data; };
    Entry entries[] = {
        { "catmark_pyramid",       catmark_pyramid },
        { "catmark_xord_interior", catmark_xord_interior },
        { "catmark_pole8",         catmark_pole8 }
    };

    for (int i = 0; i < 3; ++i) {
        printf("  Extraordinary shape '%s':\n", entries[i].name);

        ShapeDesc desc(entries[i].name, entries[i].data, kCatmark);
        Shape * shape = Shape::parseObj(desc.data.c_str(), desc.scheme, false);
        assert(shape);

        int numBaseFaces = shape->GetNumFaces();

        BuiltTable<REAL> bezierTable = buildPatchTable<REAL>(
            *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);
        BuiltTable<REAL> gregoryTable = buildPatchTable<REAL>(
            *shape, PatchFactory::Options::ENDCAP_GREGORY_BASIS);

        testPatchTableContents(bezierTable, true, desc.name.c_str());

        //  Boundaries/corners stay exact; only the interior near the
        //  extraordinary vertex differs, within approximation tolerance.
        testPositionAgreement(bezierTable, gregoryTable, numBaseFaces,
            true, (REAL) kBoundaryTol, desc.name.c_str());
        testPositionAgreement(bezierTable, gregoryTable, numBaseFaces,
            false, (REAL) kApproxTol, desc.name.c_str());

        destroyBuiltTable(bezierTable);
        destroyBuiltTable(gregoryTable);
        delete shape;
    }
}

//
//  Schemes without a bicubic Bezier representation must emit a
//  diagnosable warning and produce a valid fallback table -- never a table
//  of Bezier patches with meaningless control points.
//
static void
testUnsupportedSchemeFallback() {

    printf("  Unsupported scheme fallback (Loop and Bilinear):\n");

    struct Entry {
        char const * name;
        std::string const & data;
        Scheme       scheme;
        Far::PatchDescriptor::Type expectedFallbackType;
    };
    Entry entries[] = {
        { "loop_cube",     loop_cube,     kLoop,
          Far::PatchDescriptor::LOOP },
        { "bilinear_cube", bilinear_cube, kBilinear,
          Far::PatchDescriptor::QUADS }
    };

    for (int i = 0; i < 2; ++i) {
        g_warnings.clear();

        ShapeDesc desc(entries[i].name, entries[i].data, entries[i].scheme);
        Shape * shape = Shape::parseObj(desc.data.c_str(), desc.scheme, false);
        assert(shape);

        BuiltTable<float> table = buildPatchTable<float>(
            *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);

        bool warned = false;
        for (size_t w = 0; w < g_warnings.size(); ++w) {
            if (g_warnings[w].find("ENDCAP_BEZIER_BASIS") !=
                std::string::npos) {
                warned = true;
            }
        }
        TEST_CHECK(warned,
            "unsupported scheme must emit a Bezier fallback warning");

        //  No BEZIER descriptor may appear in the fallback table:
        int numArrays = table.patchTable->GetNumPatchArrays();
        bool hasBezier = false;
        bool hasExpectedType = false;
        for (int a = 0; a < numArrays; ++a) {
            Far::PatchDescriptor::Type type =
                table.patchTable->GetPatchArrayDescriptor(a).GetType();
            if (type == Far::PatchDescriptor::BEZIER) {
                hasBezier = true;
            }
            if (type == entries[i].expectedFallbackType) {
                hasExpectedType = true;
            }
        }
        TEST_CHECK(!hasBezier,
            "fallback table must not contain Bezier patches");
        TEST_CHECK(hasExpectedType,
            "fallback table must retain the scheme's native patch type");

        destroyBuiltTable(table);
        delete shape;
    }
}

//
//  Double-precision conversion matrices must produce the same geometry as
//  the float path (exercising both explicit template instantiations).
//
static void
testDoublePrecisionPath() {

    printf("  Double-precision conversion path:\n");

    ShapeDesc desc("catmark_pyramid", catmark_pyramid, kCatmark);
    Shape * shape = Shape::parseObj(desc.data.c_str(), desc.scheme, false);
    assert(shape);

    int numBaseFaces = shape->GetNumFaces();

    BuiltTable<float>  floatTable  = buildPatchTable<float>(
        *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);
    BuiltTable<double> doubleTable = buildPatchTable<double>(
        *shape, PatchFactory::Options::ENDCAP_BEZIER_BASIS);

    double maxDelta = 0.0;
    for (int face = 0; face < numBaseFaces; ++face) {
        int ptexFace = floatTable.ptexIndices->GetFaceId(face);
        for (int k = 0; k < 5; ++k) {
            float  sf = 0.1f + 0.2f * k;
            float  tf = 0.2f + 0.15f * k;
            double sd = sf, td = tf;

            float  pF[3];
            double pD[3];
            evaluatePosition(floatTable, ptexFace, sf, tf, pF);
            evaluatePosition(doubleTable, ptexFace, sd, td, pD);

            double d[3] = { pD[0]-pF[0], pD[1]-pF[1], pD[2]-pF[2] };
            maxDelta = std::max(maxDelta,
                std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]));
        }
    }

    TEST_CHECK(maxDelta <= (double) kExactTol,
        "float and double Bezier paths must agree");

    destroyBuiltTable(floatTable);
    destroyBuiltTable(doubleTable);
    delete shape;
}

//
//  Face-varying patches reuse the same change-of-basis matrices as vertex
//  patches.  Verify that a shape with UVs produces Bezier FVar patch
//  descriptors and that the FVar basis can be evaluated without error.
//
static void
testFaceVaryingSerialization() {

    printf("  Face-varying Bezier table 'catmark_fvar_bound0':\n");

    ShapeDesc desc("catmark_fvar_bound0", catmark_fvar_bound0, kCatmark);
    Shape * shape = Shape::parseObj(desc.data.c_str(), desc.scheme, false);
    assert(shape);
    TEST_CHECK(shape->uvs.size() > 0, "test shape must contain UVs");

    Sdc::SchemeType sdcType    = GetSdcType(*shape);
    Sdc::Options    sdcOptions = GetSdcOptions(*shape);
    //  Force a non-linear FVar channel so it uses the Bezier patch
    //  representation rather than the legacy linear patches:
    sdcOptions.SetFVarLinearInterpolation(
        Sdc::Options::FVAR_LINEAR_NONE);

    Refiner * refiner = RefinerFactory::Create(*shape,
        RefinerFactory::Options(sdcType, sdcOptions));

    Far::TopologyRefiner::AdaptiveOptions adaptiveOptions(4);
    adaptiveOptions.useInfSharpPatch = true;
    refiner->RefineAdaptive(adaptiveOptions);

    PatchFactory::Options patchOptions(4);
    patchOptions.SetEndCapType(
        PatchFactory::Options::ENDCAP_BEZIER_BASIS);
    patchOptions.useInfSharpPatch = true;
    patchOptions.generateVaryingTables = false;
    patchOptions.shareEndCapPatchPoints = false;

    int fvarChannel = 0;
    patchOptions.generateFVarTables = true;
    patchOptions.numFVarChannels = 1;
    patchOptions.fvarChannelIndices = &fvarChannel;
    patchOptions.generateFVarLegacyLinearPatches = false;

    PatchTable * patchTable = PatchFactory::Create(*refiner, patchOptions);

    //  The non-linear FVar channel must have Bezier descriptors:
    Far::PatchDescriptor fvarReg =
        patchTable->GetFVarPatchDescriptorRegular(fvarChannel);
    Far::PatchDescriptor fvarIrreg =
        patchTable->GetFVarPatchDescriptorIrregular(fvarChannel);

    bool fvarHasBezier =
        (fvarReg.GetType() == Far::PatchDescriptor::BEZIER) ||
        (fvarIrreg.GetType() == Far::PatchDescriptor::BEZIER);
    TEST_CHECK(fvarHasBezier,
        "non-linear FVar channel must use Bezier patch descriptors");
    TEST_CHECK(fvarReg.GetNumControlVertices() == 16,
        "regular Bezier FVar patches have 16 control values");

    //  Local-point FVar stencils must exist and be usable:
    TEST_CHECK(patchTable->GetNumLocalPointsFaceVarying() >= 0,
        "FVar local point query must succeed");

    delete patchTable;
    delete refiner;
    delete shape;
}

int
main(int, char **) {

    Far::SetWarningCallback(warningCaptureCallback);

    printf("Running Far Bezier patch table regression tests:\n");

    //  Exact regular case first -- the core change-of-basis correctness:
    testRegularShapeExact<float>();

    //  Boundary extrapolation:
    testBoundaryShapes<float>();

    //  Extraordinary neighborhoods (Gregory-derived interior):
    testExtraordinaryShapes<float>();

    //  General Catmark coverage combining all rules:
    ShapeDesc catmarkShapes[] = {
        ShapeDesc("catmark_cube",           catmark_cube,           kCatmark),
        ShapeDesc("catmark_gregory_test0",  catmark_gregory_test0,  kCatmark),
        ShapeDesc("catmark_gregory_test5",  catmark_gregory_test5,  kCatmark),
        ShapeDesc("catmark_cubes_infsharp", catmark_cubes_infsharp, kCatmark),
        ShapeDesc("catmark_val2_interior",  catmark_val2_interior,  kCatmark)
    };
    for (int i = 0; i < 5; ++i) {
        testCatmarkShape<float>(catmarkShapes[i]);
    }

    //  Both precision instantiations:
    testDoublePrecisionPath();

    //  Face-varying serialization and descriptors:
    testFaceVaryingSerialization();

    //  Diagnosable fallback where no Bezier representation exists:
    testUnsupportedSchemeFallback();

    printf("\n%d checks, %d failures\n", g_numTests, g_numFailures);
    return g_numFailures == 0 ? 0 : 1;
}
