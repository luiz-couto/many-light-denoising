#include "renderer.h"
#include "scene.h"
#include <cfloat>
#include <vector>
#include <thread>
#include <functional>

Renderer::Renderer() {
  film.init(Config::WIDTH, Config::HEIGHT);
}

void Renderer::render() {
  film.clear();

  int numThreads = (int)std::thread::hardware_concurrency();
  std::vector<MTRandom> samplers(numThreads);
  std::atomic<unsigned int> tileId(0);

  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  for (int i = 0; i < numThreads; i++) {
    threads.emplace_back(&Renderer::renderTile, this, i, std::ref(tileId), std::ref(samplers[i]));
  }

  for (auto& t : threads) t.join();

  film.incrementSPP();
}

void Renderer::renderTile(int threadId, std::atomic<unsigned int>& tileId, MTRandom& sampler) {
  int tilesX = (Config::WIDTH  + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int tilesY = (Config::HEIGHT + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int totalTiles = tilesX * tilesY;

  while (true) {
    unsigned int tile = tileId++;
    if ((int)tile >= totalTiles) break;

    int tileCol = tile % tilesX;
    int tileRow = tile / tilesX;
    int xStart = tileCol * Config::TILE_SIZE;
    int yStart = tileRow * Config::TILE_SIZE;
    int xEnd = std::min(xStart + Config::TILE_SIZE, Config::WIDTH);
    int yEnd = std::min(yStart + Config::TILE_SIZE, Config::HEIGHT);

    for (int y = yStart; y < yEnd; y++) {
      for (int x = xStart; x < xEnd; x++) {
        Ray ray = scene.camera.generateRay(x + 0.5f, y + 0.5f);
        IntersectionData hit = scene.traverse(ray);

        Colour colour(0.0f, 0.0f, 0.0f);
        if (hit.t < FLT_MAX) {
          ShadingData sd = scene.calculateShadingData(hit, ray);
          colour = Colour(
            sd.sNormal.x * 0.5f + 0.5f,
            sd.sNormal.y * 0.5f + 0.5f,
            sd.sNormal.z * 0.5f + 0.5f
          );
        }

        film.splat((float)x, (float)y, colour);
      }
    }
  }
}
