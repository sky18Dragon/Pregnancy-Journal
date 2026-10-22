#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include "domain_repository_records.h"
#include "sd_repository_request.h"
#include "sd_recovery_loader.h"
#include "storage_crc32.h"
#include "sd_journal_payload.h"
int main() {
 using namespace sticky_storage;
 using namespace pregnancy_repository;
 StickyDeviceSettings a{},b{}; b.last_app=StickyAppId::Settings;
 EncodedPayload pa{},pb{};
 assert(encode_device_preferences(a,pa)==RecordCodecStatus::Ok);
 assert(encode_device_preferences(b,pb)==RecordCodecStatus::Ok);
 char na[13]{},nb[13]{};
 assert(sd_repository_object_name(pa.bytes.data(),pa.size,na));
 assert(sd_repository_object_name(pb.bytes.data(),pb.size,nb));
 std::printf("different_payloads=%d first_name=%s second_name=%s\n",std::memcmp(pa.bytes.data(),pb.bytes.data(),pa.size)!=0,na,nb);
 char tmp[]="/tmp/sticky_crc_review_XXXXXX"; char* root=mkdtemp(tmp); assert(root);
 std::string app=std::string(root)+"/PREGJNL", records=app+"/RECORDS",journal=app+"/JOURNAL";
 SdRepositoryRequestPaths paths{app.c_str(),records.c_str(),{app.c_str(),journal.c_str()}};
 SdRepositoryRequestContext ctx{}; ctx.card_state=SdCardState::ReadyReadWrite;ctx.mount_generation=1;ctx.capacity_known=true;ctx.free_bytes=1024*1024;ctx.manifest_state=SdManifestProbeState::Ready;ctx.manifest_selected=true;ctx.recovery_view_ready=true;
 assert(sd_manifest_make("0123456789abcdef0123456789abcdef",1,ctx.manifest));
 RepositoryKey key{};assert(device_preferences_key(key));
 RepositoryMutation mut{};SdRepositoryRequest req{};SdRepositoryRecoveryIndex index;
 assert(repository_mutation_make_upsert(key,0,pa.bytes.data(),pa.size,mut));
 assert(sd_repository_make_commit_request(1,ctx.manifest.dataset_id,mut,1,req));
 auto first=sd_repository_execute_request(req,ctx,paths,index);
 assert(first.status==SdRepositoryRequestStatus::Ok);
 assert(repository_mutation_make_upsert(key,1,pb.bytes.data(),pb.size,mut));
 assert(sd_repository_make_commit_request(1,ctx.manifest.dataset_id,mut,2,req));
 auto second=sd_repository_execute_request(req,ctx,paths,index);
 std::printf("first_commit=%u second_commit=%u object_status=%u last_tx=%llu\n",unsigned(first.status),unsigned(second.status),unsigned(second.object_status),(unsigned long long)index.last_transaction_id());
 assert(second.status==SdRepositoryRequestStatus::Ok);
 assert(index.last_transaction_id()==2);
 assert(std::strcmp(na,nb)!=0);
 // Both valid encoded records have the same CRC residue; retry must compare bytes.
 assert(crc32(pa.bytes.data(),pa.size)==crc32(pb.bytes.data(),pb.size));
 assert(repository_mutation_make_upsert(key,1,pa.bytes.data(),pa.size,mut));
 assert(sd_repository_make_commit_request(1,ctx.manifest.dataset_id,mut,2,req));
 assert(sd_repository_execute_request(req,ctx,paths,index).status==SdRepositoryRequestStatus::TransactionConflict);
 // Force a filename collision using a pre-existing immutable object.
 WeightRecord weight{}; weight.id=1; weight.date={2026,10,10}; weight.weight_tenths_kg=600;
 EncodedPayload pw{}; assert(encode_weight(weight,pw)==RecordCodecStatus::Ok);
 char nw[13]{}; assert(sd_repository_object_name(pw.bytes.data(),pw.size,nw));
 std::string collision=records+"/"+nw;
 assert(sd_record_object_write({app.c_str(),records.c_str(),collision.c_str()},pa.bytes.data(),pa.size).status==SdRecordObjectWriteStatus::Ok);
 RepositoryKey wk{}; assert(weight_key(1,wk));
 assert(repository_mutation_make_upsert(wk,0,pw.bytes.data(),pw.size,mut));
 assert(sd_repository_make_commit_request(1,ctx.manifest.dataset_id,mut,3,req));
 assert(sd_repository_execute_request(req,ctx,paths,index).status==SdRepositoryRequestStatus::Ok);
 assert(sd_repository_execute_request(req,ctx,paths,index).status==SdRepositoryRequestStatus::Ok);
 assert(sd_record_object_probe(collision.c_str(),pa.bytes.data(),pa.size).status==SdRecordObjectProbeStatus::Match);
 // Restart recovery verifies both live objects and reads the collided-name record.
 SdRecoveryWorkspace workspace; std::string snapshots=app+"/INDEX";
 auto restored=sd_recovery_load({app.c_str(),journal.c_str(),snapshots.c_str(),records.c_str()},ctx.manifest.dataset_id,index,workspace);
 assert(restored.view_ready && restored.live_records==2 && restored.last_transaction_id==3);
 assert(sd_repository_make_read_request(1,ctx.manifest.dataset_id,wk,req));
 auto read=sd_repository_execute_request(req,ctx,paths,index);
 assert(read.status==SdRepositoryRequestStatus::Ok && read.record.payload_size==pw.size);
 assert(std::memcmp(read.record.payload.data(),pw.bytes.data(),pw.size)==0);
 // A dataset produced by the old firmware keeps its CRC-named object and
 // explicit transaction retries continue to use that journal reference.
 std::filesystem::remove_all(app);
 SdJournalTransactionPayload legacy{};
 legacy.mutation_count=1;
 auto &ref=legacy.mutations[0];
 ref.type=RepositoryMutationType::Upsert; ref.key=key;
 ref.expected_revision=0; ref.committed_revision=1;
 ref.object_bytes=pa.size; ref.object_crc32=crc32(pa.bytes.data(),pa.size);
 std::strcpy(ref.object_file,"2144DF1C.BIN");
 std::string old=records+"/2144DF1C.BIN";
 assert(sd_record_object_write({app.c_str(),records.c_str(),old.c_str()},pa.bytes.data(),pa.size).status==SdRecordObjectWriteStatus::Ok);
 std::array<uint8_t,kSdJournalMaxPayloadBytes> journal_payload{};
 auto count=sd_journal_encode_transaction_payload(legacy,journal_payload.data(),journal_payload.size());
 assert(count>0);
 assert(sd_journal_store_append(paths.journal,ctx.manifest.dataset_id,SdJournalRecordType::Commit,1,journal_payload.data(),count).status==SdJournalStoreStatus::Ready);
 restored=sd_recovery_load({app.c_str(),journal.c_str(),snapshots.c_str(),records.c_str()},ctx.manifest.dataset_id,index,workspace);
 assert(restored.view_ready && restored.last_transaction_id==1);
 assert(repository_mutation_make_upsert(key,0,pa.bytes.data(),pa.size,mut));
 assert(sd_repository_make_commit_request(1,ctx.manifest.dataset_id,mut,1,req));
 assert(sd_repository_execute_request(req,ctx,paths,index).status==SdRepositoryRequestStatus::Ok);
 assert(repository_mutation_make_upsert(key,1,pb.bytes.data(),pb.size,mut));
 assert(sd_repository_make_commit_request(1,ctx.manifest.dataset_id,mut,2,req));
 assert(sd_repository_execute_request(req,ctx,paths,index).status==SdRepositoryRequestStatus::Ok);
 assert(sd_record_object_probe(old.c_str(),pa.bytes.data(),pa.size).status==SdRecordObjectProbeStatus::Match);
 std::filesystem::remove_all(root);
}
