// SPDX-License-Identifier: MIT
#include "OpenWith.h"
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <StringView.h>
#include <Window.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
namespace {
constexpr uint32 kLoaded='owld', kOpen='owop', kOther='owot', kChosen='owch', kReady='owrd', kCanQuit='owcq';
std::mutex windowsMutex;
std::vector<BMessenger> windows;
struct AppRow : BRow {
  Lister::OpenApplication app;
  AppRow(const Lister::OpenApplication &value, bool preferred) : app(value) {
    SetField(new BStringField((app.name + (preferred ? " (preferred)" : "")).c_str()), 0);
    SetField(new BStringField(app.path.c_str()), 1);
  }
};
struct Delivery {
  std::mutex mutex;
  std::shared_ptr<Lister::OpenWithSession> session;
  Lister::OpenTypeInfo info;
  std::string error;
  std::unique_ptr<Lister::OpenApplication> chosen;
};
class OpenWithWindow : public BWindow {
  std::shared_ptr<Delivery> delivery = std::make_shared<Delivery>();
  std::shared_ptr<Lister::OpenWithSession> session;
  BColumnListView *apps;
  BStringView *type, *status;
  BButton *open, *other;
  std::unique_ptr<BFilePanel> picker;
  bool busy = true;
  static BStringView *Line(const char *name, const char *text) {
    auto line = new BStringView(name, text);
    line->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    line->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    line->SetTruncation(B_TRUNCATE_MIDDLE);
    line->SetToolTip(text);
    return line;
  }
  void Status(const std::string &text) {status->SetText(text.c_str());status->SetToolTip(text.c_str());}
  void Controls() {open->SetEnabled(!busy && session && apps->CurrentSelection());other->SetEnabled(!busy && session);}
  void Colors() {
    apps->SetColor(B_COLOR_BACKGROUND, ui_color(B_DOCUMENT_BACKGROUND_COLOR));
    apps->SetColor(B_COLOR_TEXT, ui_color(B_DOCUMENT_TEXT_COLOR));
    apps->SetColor(B_COLOR_SELECTION, ui_color(B_CONTROL_HIGHLIGHT_COLOR));
    apps->SetColor(B_COLOR_SELECTION_TEXT, ui_color(B_CONTROL_TEXT_COLOR));
    apps->SetColor(B_COLOR_NON_FOCUS_SELECTION, tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), .8f));
    apps->Invalidate();
  }
  void Work(uint32 done, std::function<void(Delivery &)> action) {
    busy = true; Controls();
    auto holder = delivery; BMessenger target(this);
    std::thread([holder, target, done, action] {
      {std::lock_guard<std::mutex> lock(holder->mutex);
       holder->error.clear();
       try { action(*holder); } catch (const std::exception &e) {holder->error = e.what();}}
      BMessage result(done); target.SendMessage(&result);
    }).detach();
  }
public:
  explicit OpenWithWindow(const std::string &path)
    : BWindow(BRect(140,120,820,490), "Listi: Open with", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS) {
    auto location = Line("open path", path.c_str());
    type = Line("open type", "Reading file type...");
    apps = new BColumnListView("open applications", B_WILL_DRAW | B_NAVIGABLE, B_FANCY_BORDER);
    apps->AddColumn(new BStringColumn("Application", 210, 90, 600, B_TRUNCATE_END), 0);
    apps->AddColumn(new BStringColumn("Location", 390, 100, 1400, B_TRUNCATE_MIDDLE), 1);
    apps->SetSelectionMode(B_SINGLE_SELECTION_LIST);
    apps->SetSelectionMessage(new BMessage(kReady));
    apps->SetInvocationMessage(new BMessage(kOpen));
    apps->SetTarget(this);apps->SetExplicitMinSize(BSize(300,130));
    open = new BButton("Open once", new BMessage(kOpen));
    other = new BButton("Choose application...", new BMessage(kOther));
    auto close = new BButton("Close", new BMessage(B_QUIT_REQUESTED));
    status = Line("open status", "Reading file type and registered applications...");
    BLayoutBuilder::Group<>(this, B_VERTICAL, 8).SetInsets(10)
      .Add(location).Add(type).Add(apps)
      .Add(Line("open scope", "This opening only. File types and default applications are unchanged."))
      .AddGroup(B_HORIZONTAL, 8).Add(other).AddGlue().Add(open).Add(close).End().Add(status);
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    {std::lock_guard<std::mutex> lock(windowsMutex);windows.emplace_back(this);}
    Controls();Colors();Show();
    Work(kLoaded, [path](auto &d) {d.session=std::make_shared<Lister::OpenWithSession>(path);d.info=d.session->Inspect();});
  }
  ~OpenWithWindow() override {
    picker.reset();
    std::lock_guard<std::mutex> lock(windowsMutex);
    windows.erase(std::remove(windows.begin(), windows.end(), BMessenger(this)), windows.end());
  }
  bool QuitRequested() override {
    if (busy) {Status("An application request is running. Please wait before closing.");return false;}
    return true;
  }
  void MessageReceived(BMessage *message) override {
    switch (message->what) {
    case kCanQuit: {
      bool allowed=QuitRequested();if(!allowed)Activate();
      BMessage reply(B_REPLY);reply.AddBool("allow",allowed);message->SendReply(&reply);break;
    }
    case kLoaded: {
      busy=false;std::lock_guard<std::mutex> lock(delivery->mutex);
      if (!delivery->error.empty()) {Status(delivery->error);Controls();break;}
      session=delivery->session;
      auto &info=delivery->info;
      std::string text=info.mime+(info.inferred?" (detected, not saved)":" (stored)");
      if(!info.description.empty())text+=" | "+info.description;
      type->SetText(text.c_str());type->SetToolTip(text.c_str());
      for(auto &app:info.applications)apps->AddRow(new AppRow(app,app.signature==info.preferred));
      if(apps->CountRows())apps->AddToSelection(apps->RowAt(0));
      Status(apps->CountRows()?"Choose an application, then Open once.":"No registered application found. Choose an application explicitly.");
      Controls();break;
    }
    case kReady:Controls();break;
    case kOther: {
      if(busy || !session)break;
      if(!picker) {
        BMessenger target(this);entry_ref directory;get_ref_for_path("/boot/system/apps",&directory);
        BMessage chosen(kChosen);
        picker=std::make_unique<BFilePanel>(B_OPEN_PANEL,&target,&directory,B_FILE_NODE,false,&chosen);
        picker->Window()->SetTitle("Listi: Choose application");
      }
      picker->Show();break;
    }
    case kChosen: {
      if(busy || !session)break;
      entry_ref ref;if(message->FindRef("refs",&ref)!=B_OK)break;
      BPath location(&ref);if(location.InitCheck()!=B_OK)break;
      std::string path=location.Path();
      Work(kChosen+1,[path](auto &d){d.session->Validate();d.chosen=std::make_unique<Lister::OpenApplication>(Lister::InspectApplication(path));});
      break;
    }
    case kChosen+1: {
      busy=false;std::lock_guard<std::mutex> lock(delivery->mutex);
      if(!delivery->error.empty())Status(delivery->error);
      else {
        // Re-choosing an existing program refreshes its identity without
        // accumulating duplicate rows on every visit to the native picker.
        for(int32 i=apps->CountRows()-1;i>=0;--i) {
          auto old=static_cast<AppRow *>(apps->RowAt(i));
          if(old->app.path==delivery->chosen->path) {apps->RemoveRow(old);delete old;}
        }
        auto row=new AppRow(*delivery->chosen,delivery->chosen->signature==delivery->info.preferred);
        apps->AddRow(row);apps->DeselectAll();apps->AddToSelection(row);
        delivery->chosen.reset();Status("Application selected. Click Open once to send the file.");
      }
      Controls();break;
    }
    case kOpen: {
      if(busy || !session)break;
      auto row=static_cast<AppRow *>(apps->CurrentSelection());if(!row)break;
      auto selected=row->app;Status("Opening selected file...");
      Work(kOpen+1,[selected](auto &d){d.session->Launch(selected);});break;
    }
    case kOpen+1: {
      busy=false;std::lock_guard<std::mutex> lock(delivery->mutex);
      Status(delivery->error.empty()?"File sent to application. Default associations are unchanged.":delivery->error);
      Controls();break;
    }
    case B_COLORS_UPDATED:BWindow::MessageReceived(message);Colors();break;
    default:BWindow::MessageReceived(message);
    }
  }
};
}
void OpenWithFile(const std::string &path) {new OpenWithWindow(path);}
bool OpenWithAllowQuit() {
  std::vector<BMessenger> current;
  {std::lock_guard<std::mutex> lock(windowsMutex);current=windows;}
  for(auto &window:current) {
    BMessage question(kCanQuit),reply;
    if(window.SendMessage(&question,&reply,500000,500000)!=B_OK) {if(window.IsValid())return false;}
    else if(!reply.GetBool("allow",false))return false;
  }
  return true;
}
