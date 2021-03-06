#pragma once
#ifndef STEMDIALOGS_DECLA_H
#define STEMDIALOGS_DECLA_H

#define EXT extern
#define INIT(s)

#include <configstorefile.h>
#define GoodConfigStoreFile ConfigStoreFile //from include.h
#ifdef WIN32
#include <CommCtrl.h> //HTREEITEM
#include <directory_tree.h>
#endif
#ifdef UNIX
#include <x/hxc.h>
#endif

#define MAX_DIALOGS 20
#define SD_REGISTER 0
#define SD_UNREGISTER 1
//---------------------------------------------------------------------------
WIN_ONLY( EXT bool StemDialog_RetDefVal; )


#if defined(SSE_COMPILER_STRUCT_391)

#pragma pack(push, STRUCTURE_ALIGNMENT)

class TStemDialog
{
private:
public:
  TStemDialog();
  ~TStemDialog(){};
  void LoadPosition(GoodConfigStoreFile*),SavePosition(bool,ConfigStoreFile*),SaveVisible(ConfigStoreFile*);

#ifdef WIN32
  void CheckFSPosition(HWND);
  void RegisterMainClass(WNDPROC,char*,int);
  static LRESULT DefStemDialogProc(HWND,UINT,WPARAM,LPARAM);
  void MakeParent(HWND),UpdateMainWindowIcon();
  void ChangeParent(HWND);
  bool IsVisible(){return Handle!=NULL;}
  inline bool HasHandledMessage(MSG *);
  inline bool HandleIsInvalid();

  HTREEITEM AddPageLabel(char *,int);
  void DestroyCurrentPage();
  void GetPageControlList(DynamicArray<HWND> &);
  void ShowPageControls(),SetPageControlsFont();

  void UpdateDirectoryTreeIcons(DirectoryTree*);

  HWND Handle,Focus,PageTree;
  HFONT Font;
  EasyStr Section;
  int nMainClassIcon;
#elif defined(UNIX)
  EasyStr Section;
  bool IsVisible(){ return Handle!=0;}
  Pixmap IconPixmap;
  Pixmap IconMaskPixmap;

  void StandardHide();

  bool StandardShow(int w,int h,char* name,
      int icon_index,long input_mask,LPWINDOWPROC WinProc,bool=false);


  Window Handle;
#endif
  int Left,Top;
  int FSLeft,FSTop;
};

#pragma pack(pop, STRUCTURE_ALIGNMENT)

#else

class TStemDialog
{
private:
public:
  TStemDialog();
  ~TStemDialog(){};
  void LoadPosition(GoodConfigStoreFile*),SavePosition(bool,ConfigStoreFile*),SaveVisible(ConfigStoreFile*);

#ifdef WIN32
  void CheckFSPosition(HWND);
  void RegisterMainClass(WNDPROC,char*,int);
  static LRESULT DefStemDialogProc(HWND,UINT,WPARAM,LPARAM);
  void MakeParent(HWND),UpdateMainWindowIcon();
  void ChangeParent(HWND);
  bool IsVisible(){return Handle!=NULL;}
  inline bool HasHandledMessage(MSG *);
  inline bool HandleIsInvalid();

  HTREEITEM AddPageLabel(char *,int);
  void DestroyCurrentPage();
  void GetPageControlList(DynamicArray<HWND> &);
  void ShowPageControls(),SetPageControlsFont();

  void UpdateDirectoryTreeIcons(DirectoryTree*);

  HWND Handle,Focus,PageTree;
  HFONT Font;
  int nMainClassIcon;
#elif defined(UNIX)
  bool IsVisible(){ return Handle!=0;}
  Pixmap IconPixmap;
  Pixmap IconMaskPixmap;

  void StandardHide();

  bool StandardShow(int w,int h,char* name,
      int icon_index,long input_mask,LPWINDOWPROC WinProc,bool=false);


  Window Handle;
#endif

  int Left,Top;
  int FSLeft,FSTop;
  EasyStr Section;
};

#endif//#if defined(SSE_COMPILER_STRUCT_391)

//---------------------------------------------------------------------------
EXT TStemDialog *DialogList[MAX_DIALOGS];
EXT int nStemDialogs;

#ifdef WIN32
// For some reason in 24-bit and 32-bit screen modes on XP ILC_COLOR24 and
// ILC_COLOR32 icons don't highlight properly, have to be 16-bit.
EXT const UINT BPPToILC[5];
#endif

#undef EXT
#undef INIT

#endif//#ifndef STEMDIALOGS_DECLA_H
