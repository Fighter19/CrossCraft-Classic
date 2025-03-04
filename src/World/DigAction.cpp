#include "DigAction.hpp"
#include "SaveData.hpp"
#include <Utilities/Input.hpp>
#include <gtx/rotate_vector.hpp>

namespace CrossCraft {

template <typename T> constexpr T DEGTORAD(T x) { return x / 180.0f * 3.14159; }

auto DigAction::doInventory(World *w) -> void {
    using namespace Stardust_Celeste::Utilities;
    auto cX = (Input::get_axis("Mouse", "X") + 1.0f) / 2.0f;
    auto cY = (Input::get_axis("Mouse", "Y") + 1.0f) / 2.0f;

    if (cX > 0.3125f && cX < 0.675f)
        cX = (cX - 0.3125f) / .04f;
    else
        return;

    if (cY > 0.3125f && cY < 0.7188f)
        cY = (cY - 0.3125f) / .08f;
    else
        return;

    int iX = cX;
    int iY = cY;

    int idx = iY * 9 + iX;

    if (idx > 41)
        return;

#if PSP || BUILD_PLAT == BUILD_VITA
    idx = (w->player->in_cursor_x) + (w->player->in_cursor_y * 9);
#endif

    w->player->itemSelections[w->player->selectorIDX] =
        w->player->inventorySelection[idx];

    return;
}

auto DigAction::dig(std::any d) -> void {
    auto w = std::any_cast<World *>(d);

    // Check that we can break
    if (w->break_icd < 0)
        w->break_icd = 0.2f;
    else
        return;

    // If we're in inventory, handle that click
    if (w->player->in_inventory) {
        doInventory(w);
        return;
    }

#if BUILD_PC
    if (w->player->in_pause) {
        if (w->player->pauseMenu->selIdx == 0) {
            w->player->in_pause = false;
        } else if (w->player->pauseMenu->selIdx == 1) {
            SaveData::save(w);
        } else if (w->player->pauseMenu->selIdx == 2) {
            exit(0);
        }
    }
#endif

    // Create a default vector of the player facing
    auto default_vec = glm::vec3(0, 0, 1);
    default_vec = glm::rotateX(default_vec, DEGTORAD(w->player->get_rot().x));
    default_vec =
        glm::rotateY(default_vec, DEGTORAD(-w->player->get_rot().y + 180));

    // Set our reach and default vector to that reach distance
    const float REACH_DISTANCE = 4.0f;
    default_vec *= REACH_DISTANCE;

    // Cast steps towards said direction
    const u32 NUM_STEPS = 50;
    for (u32 i = 0; i < NUM_STEPS; i++) {
        float percentage =
            static_cast<float>(i) / static_cast<float>(NUM_STEPS);

        auto cast_pos = w->player->get_pos() + (default_vec * percentage);

        auto ivec = glm::ivec3(static_cast<s32>(cast_pos.x),
                               static_cast<s32>(cast_pos.y),
                               static_cast<s32>(cast_pos.z));

        // Check valid vector
        if (!validate_ivec3(ivec, w->world_size))
            continue;

        // Get Block
        u32 idx = w->getIdx(ivec.x, ivec.y, ivec.z);
        auto blk = w->worldData[idx];

        // Hit through these blocks
        if (blk == Block::Air || blk == Block::Bedrock || blk == Block::Water ||
            blk == Block::Lava)
            continue;

        // We found a working block -- create break particles
        w->psystem->initialize(blk, cast_pos);

        uint16_t x = ivec.x / 16;
        uint16_t y = ivec.z / 16;
        uint32_t id = x << 16 | (y & 0x00FF);

        // Check if it's a sponge
        bool was_sponge = false;
        if (w->worldData[idx] == Block::Sponge) {
            was_sponge = true;
        }

        // Set to air
        w->worldData[idx] = Block::Air;

        // Update metadata
        int mIdx = ivec.y / 16 * w->world_size.z / 16 * w->world_size.x / 16 +
                   ivec.z / 16 * w->world_size.x / 16 + ivec.x / 16;

        w->chunksMeta[mIdx].is_full = false;

        // Send client info if multiplayer
        if (w->client != nullptr) {
            w->set_block(ivec.x, ivec.y, ivec.z, 0,
                         w->player->itemSelections[w->player->selectorIDX]);
        }

        // Update surrounding blocks on a larger radius for water filling
        if (was_sponge) {
            for (auto i = ivec.x - 3; i <= ivec.x + 3; i++) {
                for (auto j = ivec.z - 3; j <= ivec.z + 3; j++) {
                    w->add_update({i, ivec.y, j});
                    w->add_update({i, ivec.y + 1, j});
                    w->add_update({i, ivec.y - 1, j});
                    w->add_update({i, ivec.y + 2, j});
                    w->add_update({i, ivec.y - 2, j});
                }
            }
        }

        // Update Lighting
        w->update_lighting(ivec.x, ivec.z);

        if (w->chunks.find(id) != w->chunks.end())
            w->chunks[id]->generate(w);

        w->update_surroundings(ivec.x, ivec.z);
        w->update_nearby_blocks(ivec);

        break;
    }
}

} // namespace CrossCraft