#include "teton.h"
#include <cstdlib>
#include <fstream>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include "../utils/logoutput.h"
#include "../settings/EnumSettings.h"

namespace teton {

Exception::Exception(std::string message) : message_(message) {
}

void curaPolygonToPolygon(proto::Polygon* poly, cura::ConstPolygonRef curaPoly) {
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
    }

    *(poly->mutable_points()) = { points.begin(), points.end() };
}

void curaSliceLayerPartToProto(proto::LayerPart* part, const cura::SliceLayerPart& curaPart) {
    proto::Polygon* poly = part->add_polygons();

    curaPolygonToPolygon(poly, curaPart.outline.outerPolygon());

    size_t poly_id = 1;

    poly->set_id(poly_id++); // print poly bounded by exterior polygon always gets id of 1
    poly->set_type(proto::Polygon::Exterior);

    // Holes
    for (size_t i = 1; i < curaPart.outline.size(); i++) {
        proto::Polygon* hole = part->add_polygons();

        curaPolygonToPolygon(hole, curaPart.outline[i]);

        hole->set_id(poly_id++);
        hole->set_type(proto::Polygon::Hole);
    }

    // Walls
    for (size_t i = 0; i < curaPart.insets.size(); i++) {
        const cura::Polygons& wallPolys = curaPart.insets[i];
        //const cura::Polygons& wallPolys = curaPart.insets.back();

        for (size_t j = 0; j < wallPolys.size(); j++) {
            const cura::ConstPolygonRef curaInset = wallPolys[j];

            proto::Polygon* wallPoly = part->add_polygons();

            curaPolygonToPolygon(wallPoly, curaInset);

            wallPoly->set_id(poly_id++);
            wallPoly->set_type(proto::Polygon::Wall);
        }
    }

    // Skins
    for (const cura::SkinPart& skinPart : curaPart.skin_parts) {
        const cura::PolygonsPart& ppart = skinPart.outline;

        proto::Polygon* skinOuterPoly = part->add_polygons();

        curaPolygonToPolygon(skinOuterPoly, ppart[0]);

        skinOuterPoly->set_id(poly_id++);
        skinOuterPoly->set_type(proto::Polygon::Skin);

        for (size_t i = 1; i < ppart.size(); i++) {
            proto::Polygon* skinPoly = part->add_polygons();

            curaPolygonToPolygon(skinPoly, ppart[i]);

            skinPoly->set_id(poly_id++);
            skinPoly->set_type(proto::Polygon::Skin);
        }
    }

    // Infills
    const cura::Polygons& infillPolys = curaPart.getOwnInfillArea();
    for (size_t i = 0; i < infillPolys.size(); i++) {
        proto::Polygon* infillPoly = part->add_polygons();

        curaPolygonToPolygon(infillPoly, infillPolys[i]);

        infillPoly->set_id(poly_id++);
        infillPoly->set_type(proto::Polygon::Infill);
    }

    // Gaps
    for (const cura::Polygons& gaps : { curaPart.outline_gaps, curaPart.perimeter_gaps }) {
        for (size_t i = 0; i < gaps.size(); i++) {
            proto::Polygon* gapPoly = part->add_polygons();

            curaPolygonToPolygon(gapPoly, gaps[i]);

            gapPoly->set_id(poly_id++);
            gapPoly->set_type(proto::Polygon::Gap);
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

    // Mesh type
    bool infill_mesh = meshStorage.settings.get<bool>("infill_mesh");
    bool anti_overhang_mesh = meshStorage.settings.get<bool>("anti_overhang_mesh");
    bool cutting_mesh = meshStorage.settings.get<bool>("cutting_mesh");
    bool support_mesh = meshStorage.settings.get<bool>("support_mesh");
    if (infill_mesh) {
        mesh->set_type(proto::Mesh::Infill);
    } else if (cutting_mesh) {
        mesh->set_type(proto::Mesh::Cutting);
    } else if (anti_overhang_mesh || support_mesh /*|| ant others?*/) {
        mesh->set_type(proto::Mesh::UnknownMesh);
    } else {
        mesh->set_type(proto::Mesh::Normal);
    }

    // Infill line spacing
    mesh->set_infill_line_spacing(meshStorage.settings.get<cura::coord_t>("infill_line_distance"));

    // Infill pattern
    cura::EFillMethod infill_pattern = meshStorage.settings.get<cura::EFillMethod>("infill_pattern");
    switch (infill_pattern) {
        case cura::EFillMethod::GRID:
            mesh->set_infill_pattern(proto::Mesh::Grid);
            break;
        case cura::EFillMethod::TRIANGLES:
            mesh->set_infill_pattern(proto::Mesh::Triangle);
            break;
        case cura::EFillMethod::CUBIC:
            mesh->set_infill_pattern(proto::Mesh::Cubic);
            break;
        default:
            mesh->set_infill_pattern(proto::Mesh::UnknownPattern);
    }

    // Infill line angles
    std::vector<cura::AngleDegrees> infill_angles = meshStorage.settings.get< std::vector<cura::AngleDegrees> >("infill_angles");

    for (auto angle : infill_angles) {
        mesh->mutable_infill_angles()->Add(angle);
    }

    // Mesh layers
    size_t iskin = 0;
    size_t layer_id = 1;

    for (const cura::SliceLayer& l : meshStorage.layers) {
        cura::AngleDegrees skin_angle = 0.0;

        if (iskin >= meshStorage.skin_angles.size()) {
            iskin = 0;
        }

        // skin_angles shouldn't be of size 0, but this protects against that scenario
        if (iskin < meshStorage.skin_angles.size()) {
            skin_angle = meshStorage.skin_angles[iskin];
        }

        iskin++;

        if ((cura::LayerIndex)(layer_id - 1) > meshStorage.layer_nr_max_filled_layer) {
            break;
        }

        proto::Layer* layer = mesh->add_layers();

        layer->set_id(layer_id++);
        layer->set_height(l.printZ);
        layer->set_line_thickness(l.thickness);
        layer->set_line_width(line_width);
        layer->set_skin_orientation(skin_angle.value);

        size_t part_id = 1;

        for (const cura::SliceLayerPart& p : l.parts) {
            proto::LayerPart* part = layer->add_parts();

            part->set_id(part_id++);

            curaSliceLayerPartToProto(part, p);
        }
    }
}

void sliceDataStorageToTetonMeshes(const cura::SliceDataStorage& storage, std::shared_ptr<teton::proto::Meshes> meshes) {
    size_t mesh_id = 1;

    for (const cura::SliceMeshStorage& meshStorage : storage.meshes) {
        proto::Mesh* mesh = meshes->add_meshes();

        mesh->set_id(mesh_id++);

        teton::sliceMeshStorageToTetonMesh(mesh, meshStorage);
    }
}

void writeMeshesToJson(const proto::Meshes& meshes, std::string out_name) {
    std::ofstream teton_mesh_out(out_name);

    std::string meshes_string;

    google::protobuf::util::JsonPrintOptions jsonOpts;

    jsonOpts.add_whitespace = true;
    jsonOpts.always_print_primitive_fields = true;

    google::protobuf::util::MessageToJsonString(meshes, &meshes_string, jsonOpts);

    teton_mesh_out << meshes_string;
    teton_mesh_out.close();
}

} // namespace teton

