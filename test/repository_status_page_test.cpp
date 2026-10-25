#include <array>
#include <cassert>
#include "repository_status_page.h"
#include "ui_language.h"

int main() {
    using namespace sticky_storage;
    using namespace repository_status_ui;
    SdStorageServiceStatus card;
    card.running = true;
    card.accepting_manifest_requests = true;
    card.card_state = SdCardState::ReadyReadWrite;
    card.mount_generation = 7U;
    card.manifest_known = true;
    card.manifest_state = SdManifestProbeState::NotInitialized;
    const auto eligible = card;
    assert(can_initialize(card));
    for (auto value : {SdCardState::Absent, SdCardState::Mounting,
                      SdCardState::ReadyReadOnly, SdCardState::Full,
                      SdCardState::Fault, SdCardState::Ejecting, SdCardState::Unmounted}) {
        card = eligible; card.card_state = value; assert(!can_initialize(card));
    }
    for (auto value : {SdManifestProbeState::Ready, SdManifestProbeState::Degraded,
                      SdManifestProbeState::Corrupt, SdManifestProbeState::IoError}) {
        card = eligible; card.manifest_state = value; assert(!can_initialize(card));
    }
    card = eligible; card.running = false; assert(!can_initialize(card));
    card = eligible; card.accepting_manifest_requests = false; assert(!can_initialize(card));
    card = eligible; card.mount_generation = 0U; assert(!can_initialize(card));
    card = eligible; card.manifest_known = false; assert(!can_initialize(card));
    card = eligible; card.manifest_selected = true; assert(!can_initialize(card));
    const auto setup = make_state(false, false, SdRepositoryAdapterStatus::DatasetUnavailable, eligible);
    assert(setup.setup_generation == 7U);
    card = eligible; card.card_state = SdCardState::ReadyReadOnly;
    auto blocked = make_state(false, false, setup.status, card);
    assert(blocked.status == SdRepositoryAdapterStatus::CardReadOnly && blocked.setup_generation == 0U);
    card = eligible; card.manifest_state = SdManifestProbeState::Corrupt;
    blocked = make_state(false, false, setup.status, card);
    assert(blocked.status == SdRepositoryAdapterStatus::Corrupt && blocked.setup_generation == 0U);

    assert(make_state(true, false, setup.status, eligible).setup_generation == 0U);
    assert(make_state(false, true, setup.status, eligible).setup_generation == 0U);
    for (auto status : {SdRepositoryAdapterStatus::CardMissing,
                       SdRepositoryAdapterStatus::CardReadOnly, SdRepositoryAdapterStatus::Corrupt,
                       SdRepositoryAdapterStatus::StaleGeneration, SdRepositoryAdapterStatus::Ok})
        assert(make_state(false, false, status, eligible).setup_generation == 0U);

    std::array<uint8_t, 800U * 480U / 4U> pixels{};
    Canvas canvas(800U, 480U, pixels.data(), pixels.size());
    for (auto rotation : {CanvasRotation::Deg0, CanvasRotation::Deg90Clockwise}) {
        canvas.set_rotation(rotation);
        for (auto lang : {UiLanguage::English, UiLanguage::ChineseSimplified}) {
            ui_language_set(lang);
            // All three app titles use the same renderer and hit map.
            for (const char *title : {"MICROSD DATA", "WEIGHT DATA", "KICKS"}) {
                render(canvas, title, setup);
                for (auto action : {Action::Back, Action::Retry, Action::Setup}) {
                    const auto rect = target_rect(canvas, action);
                    assert(rect.x >= 0 && rect.y >= 0 && rect.width >= 48 && rect.height >= 48);
                    assert(rect.x + rect.width <= canvas.width() && rect.y + rect.height <= canvas.height());
                    assert(action_at(canvas, setup, rect.x + rect.width / 2, rect.y + rect.height / 2) == action);
                }
                const auto primary = setup_rect(canvas), retry = retry_rect(canvas);
                assert(primary.y + primary.height < retry.y);
                auto unavailable = setup; unavailable.setup_generation = 0U;
                assert(action_at(canvas, unavailable, primary.x + 1, primary.y + 1) == Action::None);
                auto pending = setup; pending.pending = true;
                assert(action_at(canvas, pending, retry.x + 1, retry.y + 1) == Action::None);
            }
        }
    }
}
