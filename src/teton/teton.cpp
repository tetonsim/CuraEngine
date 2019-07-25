#include "teton.h"
#include <fstream>
#include <google/protobuf/text_format.h>

#include "../utils/logoutput.h"

namespace teton {

Exception::Exception(std::string message) : message_(message) {
}

/*
void addSliceLayerPartToPolygons(cura::proto::Layer& layer, const cura::SliceLayerPart& sliceLayerPart) {
    cura::proto::Polygon* polygon = layer.add_polygons();

    cura::log("---------- # infill_area: %i", sliceLayerPart.infill_area_per_combine_per_density.size());
}

void sliceLayerToProtoLayer(cura::proto::MeshLayers& layers, const cura::SliceLayer& sliceLayer) {
    cura::proto::Layer* layer = layers.add_layers();

    layer->set_height(sliceLayer.printZ);
    layer->set_thickness(sliceLayer.thickness);

    for (const auto& slp : sliceLayer.parts) {
        addSliceLayerPartToPolygons(*layer, slp);
    }
}

cura::proto::MeshLayers sliceMeshToProtoLayers(const cura::SliceMeshStorage& meshStorage) {
    cura::proto::MeshLayers layers;

    for (const auto& l : meshStorage.layers) {
        sliceLayerToProtoLayer(layers, l);
    }

    return layers;
}

void writeSliceData(const cura::SliceDataStorage& storage) {
    if (storage.meshes.size() != 1) {
        throw Exception("Teton/Chop does not support more than one mesh at a time");
    }

    auto layers = sliceMeshToProtoLayers(storage.meshes.at(0));

    // serialize to file? do we need a new Protobuf type with repeated Layer?
    std::ofstream layerFile("meshLayers.proto");
    layers.SerializeToOstream(&layerFile);
    layerFile.close();
}
*/

proto::Polygon* curaPolygonToProto(cura::ConstPolygonRef polygon) {
    proto::Polygon* p = new proto::Polygon();

    // The points in the proto::Polygon are defined as a single sequence
    // of coord_ts where the values are ordered as such:
    //
    // 1. X coord of the first point
    // 2. Y coord of the first point
    // 3. X coord of the second point
    // ...
    // N. Y coord of the last point

    std::vector<cura::coord_t> points;

    points.reserve( polygon.size() * 2 );

    for (const auto& p : polygon) {
        points.push_back(p.X);
        points.push_back(p.Y);
    }

    p->set_points(points.data(), sizeof(cura::coord_t) * points.size());

    return p;
}

proto::Mesh sliceMeshStorageToTetonMesh(const cura::SliceMeshStorage& meshStorage) {
    proto::Mesh mesh;

    mesh.set_name(meshStorage.mesh_name);

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

    for (const cura::SliceLayer& l : meshStorage.layers) {
        proto::Layer* layer = mesh.add_layers();

        layer->set_height(l.printZ);
        layer->set_line_thickness(l.thickness);
        layer->set_line_width(line_width);

        for (const cura::SliceLayerPart& p : l.parts) {
            proto::LayerPart* part = layer->add_parts();

            part->set_allocated_exterior( curaPolygonToProto(p.outline.outerPolygon()) );
        }
    }
    
    std::ofstream teton_mesh_out("teton.mesh");
    
    //teton_mesh.SerializeToOstream(&teton_mesh_out);
    //teton_mesh_out.close();

    std::string mesh_string;

    google::protobuf::TextFormat::PrintToString(mesh, &mesh_string);

    teton_mesh_out << mesh_string;
    teton_mesh_out.close();

    return mesh;
}

} // namespace teton

