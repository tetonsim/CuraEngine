#include "teton.h"
#include <cstdlib>
#include <fstream>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include "../utils/logoutput.h"

namespace teton {

Exception::Exception(std::string message) : message_(message) {
}

std::string outputDirectory() {
    const char* cura_polys_dir = std::getenv("CURA_POLY_DIR");

    if (cura_polys_dir) {
        return std::string(cura_polys_dir);
    }

    for (const char* env_name : { "TMPDIR", "TMP", "TEMP", "TEMPDIR" }) {
        const char* tmpdir = std::getenv(env_name);

        if (tmpdir) {
            return std::string(tmpdir);
        }
    }

    return std::string("/tmp");
}

std::string outputFile(const cura::SliceDataStorage& storage) {
    std::string out_name("teton-mesh.stl");

    // If there is only one non-special mesh then set the out_name to that STL
    const cura::SliceMeshStorage* nonSpecialMesh = nullptr;
    for (const cura::SliceMeshStorage& meshStorage : storage.meshes) {
        bool infill_mesh = meshStorage.settings.get<bool>("infill_mesh");
        bool anti_overhang_mesh = meshStorage.settings.get<bool>("anti_overhang_mesh");
        bool cutting_mesh = meshStorage.settings.get<bool>("cutting_mesh");
        bool support_mesh = meshStorage.settings.get<bool>("support_mesh");

        if (!infill_mesh && !anti_overhang_mesh && !cutting_mesh && !support_mesh) {
            if (nonSpecialMesh) { // already set so there are multiple non special meshes
                nonSpecialMesh = nullptr;
                break;
            }
            nonSpecialMesh = &meshStorage;
        }
    }

    if (nonSpecialMesh) {
        out_name = nonSpecialMesh->mesh_name;
    }

    size_t iext = out_name.rfind(".stl");

    if (iext == std::string::npos) {
        out_name += ".json";
    } else {
        out_name.replace(iext, 4, ".json");
    }

    std::string out_dir = outputDirectory();

    if (out_dir.size() > 0) {
        out_name = out_dir + "/" + out_name;
    }

    return out_name;
}

void curaPolygonToPrintArea(proto::PrintArea* area, cura::ConstPolygonRef curaPoly) {
    // The points in the proto::Polygon are defined as a single sequence
    // of coord_ts where the values are ordered as such:
    //
    // 1. X coord of the first point
    // 2. Y coord of the first point
    // 3. X coord of the second point
    // ...
    // N. Y coord of the last point

    std::vector<cura::coord_t> points;

    points.reserve( curaPoly.size() * 2 );

    for (const auto& p : curaPoly) {
        points.push_back(p.X);
        points.push_back(p.Y);
        //points.push_back(p.X / 1000);
        //points.push_back(p.Y / 1000);
    }

    //area->set_points(points.data(), sizeof(cura::coord_t) * points.size());
    *(area->mutable_points()) = { points.begin(), points.end() };
}

void curaSliceLayerPartToProto(proto::LayerPart* part, const cura::SliceLayerPart& curaPart) {
    proto::PrintArea* area = part->add_areas();

    curaPolygonToPrintArea(area, curaPart.outline.outerPolygon());

    size_t poly_id = 1;

    area->set_id(poly_id++); // print area bounded by exterior polygon always gets id of 1
    area->set_inside(0); // exterior polygon is not inside of any other area
    area->set_type(proto::PrintArea::Exterior);

    // Holes
    for (size_t i = 1; i < curaPart.outline.size(); i++) {
        proto::PrintArea* hole = part->add_areas();

        curaPolygonToPrintArea(hole, curaPart.outline[i]);

        hole->set_id(poly_id++);
        hole->set_inside(0); // TODO
        hole->set_type(proto::PrintArea::Hole);
    }

    // Walls
    for (size_t i = 0; i < curaPart.insets.size(); i++) {
        const cura::Polygons& insetPolys = curaPart.insets[i];
        //const cura::Polygons& insetPolys = curaPart.insets.back();

        for (size_t j = 0; j < insetPolys.size(); j++) {
            const cura::ConstPolygonRef curaInset = insetPolys[j];

            proto::PrintArea* inset_area = part->add_areas();

            curaPolygonToPrintArea(inset_area, curaInset);

            inset_area->set_id(poly_id++);
            inset_area->set_inside(0); // TODO
            inset_area->set_type(proto::PrintArea::Wall);
        }
    }

    // Skins
    for (const cura::SkinPart& skinPart : curaPart.skin_parts) {
        const cura::PolygonsPart& ppart = skinPart.outline;

        proto::PrintArea* skinOuterArea = part->add_areas();

        curaPolygonToPrintArea(skinOuterArea, ppart[0]);

        skinOuterArea->set_id(poly_id++);
        skinOuterArea->set_inside(0); // TODO
        skinOuterArea->set_type(proto::PrintArea::Skin);

        for (size_t i = 1; i < ppart.size(); i++) {
            proto::PrintArea* skinArea = part->add_areas();

            curaPolygonToPrintArea(skinArea, ppart[i]);

            skinArea->set_id(poly_id++);
            skinArea->set_inside(0); // TODO
            skinArea->set_type(proto::PrintArea::Skin);
        }
    }

    // Infills
    const cura::Polygons& infillAreas = curaPart.getOwnInfillArea();
    for (size_t i = 0; i < infillAreas.size(); i++) {
        proto::PrintArea* infillArea = part->add_areas();

        curaPolygonToPrintArea(infillArea, infillAreas[i]);

        infillArea->set_id(poly_id++);
        infillArea->set_inside(0); // TODO
        infillArea->set_type(proto::PrintArea::Infill);
    }

    // Gaps
    for (const cura::Polygons& gaps : { curaPart.outline_gaps, curaPart.perimeter_gaps }) {
        for (size_t i = 0; i < gaps.size(); i++) {
            proto::PrintArea* gapArea = part->add_areas();

            curaPolygonToPrintArea(gapArea, gaps[i]);

            gapArea->set_id(poly_id++);
            gapArea->set_inside(0); // TODO
            gapArea->set_type(proto::PrintArea::Gap);
        }
    }
}

void sliceMeshStorageToTetonMesh(proto::Mesh* mesh, const cura::SliceMeshStorage& meshStorage) {
    mesh->set_name(meshStorage.mesh_name);

    cura::coord_t wall_line_width_0 = meshStorage.settings.get<cura::coord_t>("wall_line_width_0");
    cura::coord_t wall_line_width_x = meshStorage.settings.get<cura::coord_t>("wall_line_width_x");
    cura::coord_t skin_line_width = meshStorage.settings.get<cura::coord_t>("skin_line_width");
    cura::coord_t infill_line_width = meshStorage.settings.get<cura::coord_t>("infill_line_width");
    double initial_layer_line_width_factor = meshStorage.settings.get<double>("initial_layer_line_width_factor");

    if (initial_layer_line_width_factor != 100.0) {
        cura::logWarning("Teton: initial layer line width factor of %f will be ignored", initial_layer_line_width_factor);
    }

    cura::coord_t line_width = wall_line_width_0;

    if (wall_line_width_x != line_width) {
    }

    if (skin_line_width != line_width) {
    }

    if (infill_line_width != line_width) {
    }

    size_t layer_id = 1;

    for (const cura::SliceLayer& l : meshStorage.layers) {

        if ((cura::LayerIndex)(layer_id - 1) > meshStorage.layer_nr_max_filled_layer) {
            break;
        }

        proto::Layer* layer = mesh->add_layers();

        layer->set_id(layer_id++);
        layer->set_height(l.printZ);
        layer->set_line_thickness(l.thickness);
        layer->set_line_width(line_width);

        size_t part_id = 1;

        for (const cura::SliceLayerPart& p : l.parts) {
            proto::LayerPart* part = layer->add_parts();

            part->set_id(part_id++);

            curaSliceLayerPartToProto(part, p);
        }
    }
}

proto::Meshes sliceDataStorageToTetonMeshes(const cura::SliceDataStorage& storage) {
    proto::Meshes meshes;

    size_t mesh_id = 1;

    for (const cura::SliceMeshStorage& meshStorage : storage.meshes) {
        proto::Mesh* mesh = meshes.add_meshes();

        mesh->set_id(mesh_id++);

        teton::sliceMeshStorageToTetonMesh(mesh, meshStorage);
    }

    std::string out_name = outputFile(storage);

    std::ofstream teton_mesh_out(out_name);

    std::string meshes_string;

    google::protobuf::util::JsonPrintOptions jsonOpts;

    jsonOpts.add_whitespace = true;
    jsonOpts.always_print_primitive_fields = true;

    google::protobuf::util::MessageToJsonString(meshes, &meshes_string, jsonOpts);

    teton_mesh_out << meshes_string;
    teton_mesh_out.close();

    return meshes;
}

} // namespace teton

