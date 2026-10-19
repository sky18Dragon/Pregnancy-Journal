#pragma once

#include <cstddef>
#include <cstdint>

#include "digit_fields.h"
#include "settings_pages.h"

// Page flow of the Settings app, free of hardware: which page is shown, the
// device-time editor, the progress through the storage dialogs and what each
// action asks the rest of the system to do. The app task owns the display,
// touch and the services behind SettingsFlowPort (RTC, MicroSD manifest and
// data delete, legacy import, device binding); it drives this class and carries
// out the requests in the result. Host tests fake the port.
// 设置应用的页面流程（页面切换、时间编辑、存储对话框进度、各动作要求的操作），不接触硬件；
// RTC、MicroSD、旧数据迁移、设备绑定等服务通过接口注入。

struct SettingsClockTime {
  uint16_t year = 0U;
  uint8_t month = 0U;
  uint8_t day = 0U;
  uint8_t hour = 0U;
  uint8_t minute = 0U;
};

class SettingsFlowPort {
public:
  virtual ~SettingsFlowPort() = default;
  virtual bool read_clock(SettingsClockTime &time) = 0;
  virtual bool write_clock(const SettingsClockTime &time) = 0;

  // A fresh snapshot of the card / dataset / migration eligibility.
  virtual SettingsStoragePageState storage_state() = 0;

  // Initialization: a dataset id is generated when the confirmation opens and
  // forgotten when the dialog is left.
  virtual void prepare_initialization() = 0;
  virtual bool has_dataset_id() const = 0;
  virtual void forget_dataset_id() = 0;
  // Returns nullptr when the request was queued, otherwise the failure message.
  virtual const char *submit_initialization() = 0;
  virtual const char *safe_eject() = 0;

  // Legacy (old device) data import.
  virtual bool migration_pending() = 0;
  virtual bool migration_complete() = 0;
  virtual void begin_import() = 0;
  virtual void clear_old_copies() = 0;

  // Card data delete and device-binding reset.
  virtual void prepare_delete() = 0;
  // Returns the result message; `deleted` is true when the data is gone.
  virtual const char *delete_card_data(bool enabled, bool &deleted) = 0;
  virtual const char *reset_device_binding() = 0;
};

// What the app has to do after an action.
struct SettingsFlowResult {
  bool render = false;          // redraw the (possibly new) page
  bool render_full = false;     // ... with a forced full refresh
  bool toggle_language = false; // ask the coordinator to flip the language
  bool select_shortcut = false; // save `shortcut` as the double-click target
  StickyShortcutTarget shortcut = StickyShortcutTarget::WeightRecordToday;
  bool open_launcher = false;   // leave for the launcher (no redraw)
  bool request_home = false;    // ask for Baby Week (no redraw)
  bool request_pregnancy = false; // ask for the pregnancy setup (no redraw)
  bool notify_data_deleted = false; // card data was deleted: invalidate caches
};

class SettingsFlow {
public:
  explicit SettingsFlow(SettingsFlowPort &port) : port_(port) {}

  SettingsFlowResult handle_action(SettingsAction action);

  // Message shown on the storage pages; owned by string literals.
  const char *operation_message() const { return operation_message_; }
  void set_operation_message(const char *message) { operation_message_ = message; }
  void clear_operation_message() { operation_message_ = nullptr; }

  SettingsPage page() const { return page_; }
  const DigitFields &time_fields() const { return time_fields_; }
  bool input_error() const { return input_error_; }
  bool migrating_kicks() const { return migrating_kicks_; }
  bool editing_time() const { return page_ == SettingsPage::TimeEditor; }

private:
  void open_time_editor();
  void save_time();

  SettingsFlowPort &port_;
  SettingsPage page_ = SettingsPage::Main;
  DigitFields time_fields_{DigitFieldKind::DateTime};
  bool input_error_ = false;
  bool migrating_kicks_ = false;
  const char *operation_message_ = nullptr;
};
