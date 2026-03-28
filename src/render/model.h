/**
 * model.h — glTF 2.0 model loading via cgltf
 *
 * Loads .gltf/.glb files into an array of Mesh objects for rendering.
 */
#pragma once

#include "render/mesh.h"

#include <stdint.h>

typedef struct {
    Mesh *meshes;
    int   meshCount;
} Model;

/**
 * Load a glTF 2.0 model from file.
 *
 * @param path  Path to .gltf or .glb file
 * @return Model with allocated meshes; meshCount == 0 on failure
 */
Model model_load(const char *path);

/**
 * Draw all meshes in the model.
 *
 * @param m       Model to draw
 * @param shader  GL shader program (currently unused, for future per-mesh materials)
 */
void model_draw(const Model *m, uint32_t shader);

/** Destroy all meshes and free memory. */
void model_destroy(Model *m);
