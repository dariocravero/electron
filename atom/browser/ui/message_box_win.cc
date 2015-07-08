// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/ui/message_box.h"

#include <windows.h>
#include <commctrl.h>

#include "atom/browser/native_window_views.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/win/scoped_gdi_object.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/icon_util.h"

namespace atom {

namespace {

// Small command ID values are already taken by Windows, we have to start from
// a large number to avoid conflicts with Windows.
const int kIDStart = 100;

int ShowMessageBoxUTF16(HWND parent,
                        MessageBoxType type,
                        const std::vector<base::string16>& buttons,
                        int cancel_id,
                        const base::string16& title,
                        const base::string16& message,
                        const base::string16& detail,
                        const gfx::ImageSkia& icon) {
  std::vector<TASKDIALOG_BUTTON> dialog_buttons;
  for (size_t i = 0; i < buttons.size(); ++i)
    dialog_buttons.push_back({i + kIDStart, buttons[i].c_str()});

  TASKDIALOG_FLAGS flags = TDF_SIZE_TO_CONTENT;  // show all content.
  if (cancel_id != 0)
    flags |= TDF_ALLOW_DIALOG_CANCELLATION;  // allow dialog to be cancelled.

  TASKDIALOGCONFIG config = { 0 };
  config.cbSize         = sizeof(config);
  config.hwndParent     = parent;
  config.hInstance      = GetModuleHandle(NULL);
  config.dwFlags        = flags;
  config.pszWindowTitle = title.c_str();
  config.pButtons       = &dialog_buttons.front();
  config.cButtons       = dialog_buttons.size();

  base::win::ScopedHICON hicon;
  if (!icon.isNull()) {
    hicon.Set(IconUtil::CreateHICONFromSkBitmap(*icon.bitmap()));
    config.dwFlags |= TDF_USE_HICON_MAIN;
    config.hMainIcon = hicon.Get();
  } else {
    // Show icon according to dialog's type.
    switch (type) {
      case MESSAGE_BOX_TYPE_INFORMATION:
        config.pszMainIcon = TD_INFORMATION_ICON;
        break;
      case MESSAGE_BOX_TYPE_WARNING:
        config.pszMainIcon = TD_WARNING_ICON;
        break;
      case MESSAGE_BOX_TYPE_ERROR:
        config.pszMainIcon = TD_ERROR_ICON;
        break;
    }
  }

  // If "detail" is empty then don't make message hilighted.
  if (detail.empty()) {
    config.pszContent = message.c_str();
  } else {
    config.pszMainInstruction = message.c_str();
    config.pszContent = detail.c_str();
  }

  int id = 0;
  TaskDialogIndirect(&config, &id, NULL, NULL);
  if (id == 0 || id == IDCANCEL)
    return cancel_id;
  else
    return id - kIDStart;
}

void RunMessageBoxInNewThread(base::Thread* thread,
                              NativeWindow* parent,
                              MessageBoxType type,
                              const std::vector<std::string>& buttons,
                              int cancel_id,
                              const std::string& title,
                              const std::string& message,
                              const std::string& detail,
                              const gfx::ImageSkia& icon,
                              const MessageBoxCallback& callback) {
  int result = ShowMessageBox(parent, type, buttons, cancel_id, title, message,
                              detail, icon);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, thread);
}

}  // namespace

int ShowMessageBox(NativeWindow* parent,
                   MessageBoxType type,
                   const std::vector<std::string>& buttons,
                   int cancel_id,
                   const std::string& title,
                   const std::string& message,
                   const std::string& detail,
                   const gfx::ImageSkia& icon) {
  std::vector<base::string16> utf16_buttons;
  for (const auto& button : buttons)
    utf16_buttons.push_back(base::UTF8ToUTF16(button));

  HWND hwnd_parent = parent ?
      static_cast<atom::NativeWindowViews*>(parent)->GetAcceleratedWidget() :
      NULL;

  NativeWindow::DialogScope dialog_scope(parent);
  return ShowMessageBoxUTF16(hwnd_parent,
                             type,
                             utf16_buttons,
                             cancel_id,
                             base::UTF8ToUTF16(title),
                             base::UTF8ToUTF16(message),
                             base::UTF8ToUTF16(detail),
                             icon);
}

void ShowMessageBox(NativeWindow* parent,
                    MessageBoxType type,
                    const std::vector<std::string>& buttons,
                    int cancel_id,
                    const std::string& title,
                    const std::string& message,
                    const std::string& detail,
                    const gfx::ImageSkia& icon,
                    const MessageBoxCallback& callback) {
  scoped_ptr<base::Thread> thread(
      new base::Thread(ATOM_PRODUCT_NAME "MessageBoxThread"));
  thread->init_com_with_mta(false);
  if (!thread->Start()) {
    callback.Run(cancel_id);
    return;
  }

  base::Thread* unretained = thread.release();
  unretained->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RunMessageBoxInNewThread, base::Unretained(unretained),
                 parent, type, buttons, cancel_id, title, message, detail, icon,
                 callback));
}

void ShowErrorBox(const base::string16& title, const base::string16& content) {
}

}  // namespace atom
