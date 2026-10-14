#include <cassert>
#include <cstdint>
#include <fstream>

#include "canvas.h"
#include "manifest_commit_test_pages.h"

namespace {

void write_preview(Canvas &canvas, const char *path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << canvas.width() << " " << canvas.height()
           << "\n255\n";
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const uint8_t value =
                canvas.pixel_at(x, y) == GrayLevel::Black ? 0U : 255U;
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
        }
    }
}

}  // namespace

int main()
{
    using namespace sticky_storage;

    uint8_t framebuffer[800U * 480U / 2U] = {};
    Canvas canvas(800U, 480U, framebuffer, sizeof(framebuffer));
    ManifestCommitTestView view = {
        ManifestCommitTestPhase::Ready, 1U, 2U, 'A', 'B', 0U,
    };
    manifest_commit_test_page_render(canvas, view);
    write_preview(canvas, "/tmp/manifest_commit_ready_landscape.ppm");
    assert(manifest_commit_test_page_action_at(view, 800, 480, 400, 430) ==
           ManifestCommitTestAction::Continue);

    view.phase = ManifestCommitTestPhase::Confirm;
    manifest_commit_test_page_render(canvas, view);
    write_preview(canvas, "/tmp/manifest_commit_confirm_landscape.ppm");
    assert(manifest_commit_test_page_action_at(view, 800, 480, 100, 430) ==
           ManifestCommitTestAction::Cancel);
    assert(manifest_commit_test_page_action_at(view, 800, 480, 600, 430) ==
           ManifestCommitTestAction::WriteNext);

    canvas.set_rotation(CanvasRotation::Deg90Clockwise);
    view.phase = ManifestCommitTestPhase::Ready;
    manifest_commit_test_page_render(canvas, view);
    write_preview(canvas, "/tmp/manifest_commit_ready_portrait.ppm");
    assert(manifest_commit_test_page_action_at(view, 480, 800, 240, 748) ==
           ManifestCommitTestAction::Continue);

    view.phase = ManifestCommitTestPhase::Confirm;
    manifest_commit_test_page_render(canvas, view);
    write_preview(canvas, "/tmp/manifest_commit_confirm_portrait.ppm");
    assert(manifest_commit_test_page_action_at(view, 480, 800, 100, 748) ==
           ManifestCommitTestAction::Cancel);
    assert(manifest_commit_test_page_action_at(view, 480, 800, 360, 748) ==
           ManifestCommitTestAction::WriteNext);

    view.phase = ManifestCommitTestPhase::Pending;
    assert(manifest_commit_test_page_action_at(view, 800, 480, 400, 430) ==
           ManifestCommitTestAction::None);
    return 0;
}
