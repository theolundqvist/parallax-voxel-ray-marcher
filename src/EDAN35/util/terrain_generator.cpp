#include "./terrain.hpp"
#include <future>
#include "./ctpl_stl.h"

class TerrainGenerator {
private:
    const int nbr_threads = std::thread::hardware_concurrency() - 1;
    ctpl::thread_pool queue = ctpl::thread_pool(nbr_threads);
    typedef struct result_t {
        terrain *terrain = nullptr;
        bool retrieved = false;
    } result_t;
    std::vector<result_t> results;
    std::vector<std::future<terrain *>> futures;
    int nbr_terrains, width, depth, elevation;


public:

    TerrainGenerator(
            int width, int depth, int elevation, int nbr_terrains
    ): width(width), depth(depth), elevation(elevation), nbr_terrains(nbr_terrains) { };

    void start(){
        int width = this->width; int depth = this->depth; int elevation = this->elevation;
        results.reserve(nbr_terrains);
        // just generate one large texture
        for (int i = 0; i < 1; ++i) {
            results[i] = result_t();
            auto hm = queue.push([i, width, depth, elevation](int) {
                auto start = std::chrono::system_clock::now();
                int w = 4;
                //glm::vec2 chunk_offset = glm::vec2(i%w-w/2, i/w);
                //auto t = new terrain(width, depth,  elevation, 3.0f, 4, 0.5f, 2.0f, 1, chunk_offset);
                auto t = new terrain(width*10, depth*10,  elevation, 2.0f, 4, 0.5f, 2.0f, 1, glm::vec3(0));
                printf("Thread %d, generated terrain nbr %d in %lld ms\n", i, i, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count());
                return t;
            });
            futures.emplace_back(std::move(hm));
        }
    }


    bool terrainExists(int index) {
        if(results[0].retrieved) return true;
        auto status = futures[0].wait_for(std::chrono::microseconds(1));
        return status == std::future_status::ready;
    }

    terrain *getTerrain(int index) {
        index = 0;
        if(results[index].retrieved) return results[index].terrain;
        auto terrain = futures[index].get();
        results[index].terrain = terrain;
        results[index].retrieved = true;
        return terrain;
    }
};
