#include <cassert>
#include <cstring>
#include "device_preferences_sync.h"
using namespace sticky_storage;
using Status = SdRepositoryAdapterStatus;
namespace sticky_storage {
SdRepositoryAdapterStatus sd_repository_adapter_preflight(const SdStorageServiceStatus &s, bool) {
    return s.accepting_repository_requests ? Status::Ok : Status::StorageUnavailable;
}
}
struct Store {
    bool busy = false, valid = false, load = false;
    unsigned loads = 0, saves = 0;
    Status result = Status::Ok;
    StickyDeviceSettings disk = {}, queued = {};
    bool pending() const { return busy; }
    bool ready() const { return valid; }
    const StickyDeviceSettings &settings() const { return disk; }
    Status begin_load() { ++loads; busy = load = true; valid = false; return Status::Pending; }
    Status begin_save(const StickyDeviceSettings &s) { ++saves; queued=s; busy=true; load=false; return Status::Pending; }
    Status poll() {
        busy=false;
        if(result == Status::Ok) { valid=true; if(!load) disk=queued; }
        return result;
    }
};
int main() {
    SdDataDeleteConfirmation confirmation{};
    confirmation.mount_generation=4;
    confirmation.manifest_selected=true;
    std::strcpy(confirmation.dataset_id,"0123456789abcdef0123456789abcdef");
    assert(confirmation.matches(4,true,confirmation.dataset_id));
    assert(!confirmation.matches(5,true,confirmation.dataset_id));
    assert(!confirmation.matches(4,false,nullptr));
    assert(!confirmation.matches(4,true,"1123456789abcdef0123456789abcdef"));
    Store store; DevicePreferencesSync sync; SdStorageServiceStatus card{};
    card.mount_generation=1; card.manifest_selected=true; card.accepting_repository_requests=true;
    std::strcpy(card.selected_manifest.dataset_id,"0123456789abcdef0123456789abcdef");
    sync.tick(store,card,0); sync.tick(store,card,1);
    sync.toggle_language(); sync.tick(store,card,2);
    assert(store.saves==1);
    // In-flight snapshot does not overwrite a newer user intention.
    sync.toggle_language(); sync.set_last_app(StickyAppId::Settings);
    sync.tick(store,card,3); assert(store.saves==2);
    store.result=Status::IoError; sync.tick(store,card,4);
    assert(sync.saving_failed());
    for(unsigned i=5;i<1004;++i) sync.tick(store,card,i);
    assert(store.saves==2 && store.loads==1);
    store.result=Status::Ok; sync.tick(store,card,1004); assert(store.loads==2);
    sync.tick(store,card,1005); assert(store.saves==3);
    sync.tick(store,card,1006); assert(!sync.saving_failed());
    assert(store.disk.last_app==StickyAppId::Settings && store.disk.language==UiLanguage::English);
    // Card changes while saving: collect old completion, then load new card.
    sync.toggle_language(); sync.tick(store,card,1007);
    ++card.mount_generation; store.disk={};
    sync.tick(store,card,1008); assert(store.loads==3);
    store.disk={}; sync.tick(store,card,1009);
    assert(sync.desired().last_app==StickyAppId::Pregnancy);
    assert(sync.desired().language==UiLanguage::English);
    sync.tick(store,card,1010); assert(store.saves==4);
}
