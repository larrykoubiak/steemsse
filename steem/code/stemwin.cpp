/*---------------------------------------------------------------------------
FILE: stemwin.cpp
MODULE: Steem
DESCRIPTION: This file (included from gui.cpp) handles the main Steem window
and its various buttons.
---------------------------------------------------------------------------*/

#if defined(SSE_COMPILER_INCLUDED_CPP)
#pragma message("Included for compilation: stemwin.cpp")
#endif

#ifdef SSE_BUILD
#define LOGSECTION LOGSECTION_VIDEO_RENDERING
#else
#define LOGSECTION LOGSECTION_INIT
#endif

//---------------------------------------------------------------------------
void StemWinResize(int xo,int yo)
{
  TRACE_LOG("StemWinResize(%d,%d)\n",xo,yo);
  int res=screen_res;
  if (mixed_output)
    res=1;
  int Idx=WinSizeForRes[res];

#ifndef NO_CRAZY_MONITOR
  if (extended_monitor){
#ifdef WIN32
    int FrameWidth=GetSystemMetrics(SM_CXFRAME)*2;
#else
    int FrameWidth=0;
#endif
#if defined(SSE_VAR_RESIZE)
    SetStemWinSize(min(em_width,(WORD)(GetScreenWidth()-4-FrameWidth)),
                    min(em_height,(WORD)(GetScreenHeight()-5-MENUHEIGHT-4-30)),
                    0,0);
#else
    SetStemWinSize(min(em_width,GetScreenWidth()-4-FrameWidth),
                    min(em_height,GetScreenHeight()-5-MENUHEIGHT-4-30),
                    0,0);
#endif
  }else
#endif


#if defined(SSE_VID_ST_ASPECT_RATIO_WIN)
/*  With this little mod we have optional PAL aspect ratio in windowed mode
    too, in stretch modes.
*/
#if defined(SSE_VID_BORDERS_GUI_392)
  if (border){
#else
  if (border & 1){
#endif
    while (WinSizeBorder[res][Idx].x>GetScreenWidth()) Idx--;
    int h=WinSizeBorder[res][Idx].y;
    if(OPTION_ST_ASPECT_RATIO
#if defined(SSE_VID_ST_MONITOR_393)
      && res<2) // also non stretch, even if that's not beautiful
#else
      && res<2 && WinSizeForRes[res]==1 && draw_win_mode[res]==0)
#endif
      h*=ST_ASPECT_RATIO_DISTORTION;
    SetStemWinSize(WinSizeBorder[res][Idx].x,h,
      xo*WinSize[res][Idx].x/640,yo*WinSize[res][Idx].y/400);
  }else{
    while (WinSize[res][Idx].x>GetScreenWidth()) Idx--;
    int h=WinSize[res][Idx].y;
    if(OPTION_ST_ASPECT_RATIO 
      && res<2 && WinSizeForRes[res]==1 && draw_win_mode[res]==0)
      h*=ST_ASPECT_RATIO_DISTORTION;
    SetStemWinSize(WinSize[res][Idx].x,h,
      xo*WinSize[res][Idx].x/640,yo*WinSize[res][Idx].y/400);
  }
#else
  if (border & 1){
    while (WinSizeBorder[res][Idx].x>GetScreenWidth()) Idx--;
    SetStemWinSize(WinSizeBorder[res][Idx].x,WinSizeBorder[res][Idx].y,
      xo*WinSize[res][Idx].x/640,yo*WinSize[res][Idx].y/400);
  }else{
    while (WinSize[res][Idx].x>GetScreenWidth()) Idx--;
    SetStemWinSize(WinSize[res][Idx].x,WinSize[res][Idx].y,
      xo*WinSize[res][Idx].x/640,yo*WinSize[res][Idx].y/400);
  }
#endif

#if defined(SSE_VID_D3D)
  if(D3D9_OK && Disp.pD3DDevice)
    Disp.D3DSpriteInit(); //smooth res changes (eg in GEM)
#endif
#if defined(SSE_GUI_STATUS_BAR)
#if defined(SSE_VID_FS_GUI_392B)
  if(!FullScreen) // of course (v3.9.2), eg The Pawn
#endif
    GUIRefreshStatusBar();//of course (v3.5.5)
#endif

}
//---------------------------------------------------------------------------
void fast_forward_change(bool Down,bool Searchlight)
{
  if (Down){
    if (fast_forward<=0){
      if (runstate==RUNSTATE_STOPPED){
        PostRunMessage();
        fast_forward=RUNSTATE_STOPPED+1;
      }else if (runstate==RUNSTATE_STOPPING){
        if (fast_forward==-1) runstate=RUNSTATE_RUNNING;
        fast_forward=RUNSTATE_STOPPED+1;
      }else{
        fast_forward=1;
      }
#ifdef SSE_SOUND_16BIT_CENTRED
      Sound_Stop();
#else
      Sound_Stop(0);
#endif
    }
    flashlight(Searchlight);
  }else if (Down==0 && fast_forward){
    if (fast_forward==RUNSTATE_STOPPED+1){
      fast_forward=0;
      if (runstate==RUNSTATE_RUNNING){
        runstate=RUNSTATE_STOPPING;
        fast_forward=-1;
      }
      RunMessagePosted=0;
    }else{
      fast_forward=0;
    }
    fast_forward_stuck_down=0;
    flashlight(0);
    Sound_Start();
  }
  floppy_access_started_ff=0;
  WIN_ONLY( SendMessage(GetDlgItem(StemWin,109),BM_SETCHECK,fast_forward,1); )
  UNIX_ONLY( FastBut.set_check(fast_forward); )
}
//---------------------------------------------------------------------------
void flashlight(bool on)
{
  if (on && flashlight_flag==0){ //turn flashlight on
//    palette_convert_entry=palette_convert_entry_dont;
    for (int n=0;n<9;n++){
      PCpal[n]=colour_convert(240-n*15,255-n*15,60);
    }
    for (int n=0;n<7;n++){
      PCpal[n+9]=colour_convert(0,30+n*8,50+n*30);
    }
    flashlight_flag=true;
  }else if (on==0){
    flashlight_flag=0;
    draw_init_resdependent();
    palette_convert_all();
  }
}
//---------------------------------------------------------------------------
void slow_motion_change(bool Down)
{
  if (Down){
    if (slow_motion<=0){
      if (runstate==RUNSTATE_STOPPED){
        PostRunMessage();
        slow_motion=RUNSTATE_STOPPED+1;
      }else if (runstate==RUNSTATE_STOPPING){
        if (slow_motion==-1) runstate=RUNSTATE_RUNNING;
        slow_motion=RUNSTATE_STOPPED+1;
      }else{
        slow_motion=1;
      }
#ifdef SSE_SOUND_16BIT_CENTRED
      Sound_Stop();
#else
      Sound_Stop(0);
#endif
    }
  }else if (Down==0 && slow_motion){
    if (slow_motion==RUNSTATE_STOPPED+1){
      slow_motion=0;
      if (runstate==RUNSTATE_RUNNING){
        runstate=RUNSTATE_STOPPING;
        slow_motion=-1;
      }
      RunMessagePosted=0;
    }else{
      slow_motion=0;
    }
    Sound_Start();
  }
}
//---------------------------------------------------------------------------
#ifdef WIN32

void GetRealVKCodeForKeypad(WPARAM &wPar,LPARAM &lPar)
{
  UINT Scancode=BYTE(HIWORD(lPar));
/*
24 Indicates whether the key is an extended key, such as the right-hand ALT 
and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The
 value is 1 if it is an extended key; otherwise, it is zero. 
*/
  bool Extend=(lPar & BIT_24)!=0;
  if (Scancode==MapVirtualKey(VK_INSERT,0)) wPar=Extend ? VK_INSERT:VK_NUMPAD0;
  if (Scancode==MapVirtualKey(VK_DELETE,0)) wPar=Extend ? VK_DELETE:VK_DECIMAL;
  if (Scancode==MapVirtualKey(VK_END,0)) wPar=Extend ? VK_END:VK_NUMPAD1;
  if (Scancode==MapVirtualKey(VK_DOWN,0)) wPar=Extend ? VK_DOWN:VK_NUMPAD2;
  if (Scancode==MapVirtualKey(VK_NEXT,0)) wPar=Extend ? VK_NEXT:VK_NUMPAD3;
  if (Scancode==MapVirtualKey(VK_LEFT,0)) wPar=Extend ? VK_LEFT:VK_NUMPAD4;
  if (Scancode==MapVirtualKey(VK_CLEAR,0)) wPar=Extend ? VK_CLEAR:VK_NUMPAD5;
  if (Scancode==MapVirtualKey(VK_RIGHT,0)) wPar=Extend ? VK_RIGHT:VK_NUMPAD6;
  if (Scancode==MapVirtualKey(VK_HOME,0)) wPar=Extend ? VK_HOME:VK_NUMPAD7;
  if (Scancode==MapVirtualKey(VK_UP,0)) wPar=Extend ? VK_UP:VK_NUMPAD8;
  if (Scancode==MapVirtualKey(VK_PRIOR,0)) wPar=Extend ? VK_PRIOR:VK_NUMPAD9;
}

LRESULT PASCAL WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar)
{
  switch (Mess){
    case WM_PAINT:
    {
      RECT dest;

      GetClientRect(Win,&dest);
      int Height=dest.bottom;
      dest.bottom=MENUHEIGHT;
#if defined(SSE_VID_FS_GUI_392)
      // copy the region before BeginPaint(), which will reset it
      HRGN hRgn=0;
      int region_type=0;
      if (FullScreen){
        hRgn=CreateRectRgn(0,0,0,0);
        region_type=GetUpdateRgn(Win,hRgn,FALSE);
      }
#endif

      PAINTSTRUCT ps;
      BeginPaint(Win,&ps);

#if defined(SSE_VID_FS_GUI_392)
/*  When a dialog box is moved in the fullscreen GUI, it trashes the background.
    It's no big problem but it looks bad.
    It is possible to redraw the picture by blitting on dirty rectangles.
    In Direct3D, one call is enough:
    Disp.pD3DDevice->Present(NULL,NULL,NULL,lpRgnData);
    Unfortunately, I've only seen it work in Windows 10, not XP nor Vista.
    In DirectDraw, we need to do a blit for each rectangle. It works on
    most systems, but only in flip and straight blit modes, and not with
    Triple Buffering.
    So for a consistent experience, we erase the rectangles in all cases.
*/
      if (FullScreen && OPTION_FULLSCREEN_GUI)
      {
        if (region_type!=NULLREGION && region_type!=ERROR)
        {
          DWORD dwCount=GetRegionData(hRgn,0,NULL); // 1st call to get #bytes
          if(dwCount)
          {
            RGNDATA *lpRgnData=(RGNDATA*)new BYTE[dwCount];
            dwCount=GetRegionData(hRgn,dwCount,lpRgnData); // 2nd call to get rectangles
            if(dwCount)
            {
              HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_BACKGROUND));
              LPRECT pRect = (LPRECT)lpRgnData->Buffer;
              for(DWORD i=0;i<lpRgnData->rdh.nCount;i++)
                FillRect(ps.hdc,&pRect[ i ],br); // erase all rectangles
              DeleteObject(br);
            } //if(dwCount)
            delete [] lpRgnData;
          }//if(dwCount)
        }//if (region_type!=NULLREGION && region_type!=ERROR)
        DeleteObject(hRgn);
      }//if (FullScreen)
#endif

#ifndef ONEGAME
      //SS background for menu bar, must do that AFTER we redraw the 
      // invalidated rectangles in fullscreen mode or we get those stripes...
      HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
      FillRect(ps.hdc,&dest,br);
      DeleteObject(br);
#endif

      if (FullScreen){
        int menu_bottom=0 NOT_ONEGAME(+MENUHEIGHT+2);
#ifndef ONEGAME
        dest.bottom=menu_bottom;
        DrawEdge(ps.hdc,&dest,EDGE_RAISED,BF_BOTTOM);
#endif

        int x_gap=0,y_gap=0;
#if !defined(SSE_VID_D3D)
        if (draw_fs_blit_mode!=DFSM_LAPTOP){
#ifndef NO_CRAZY_MONITOR
          if (extended_monitor){
            x_gap=(Disp.SurfaceWidth-em_width)/2;
            y_gap=(Disp.SurfaceHeight-em_height)/2;
          }else
#endif
#if defined(SSE_VID_BORDERS_GUI_392)
          if (border){
#else
          if (border & 1){
#endif
            x_gap=(800 - (BORDER_SIDE+320+BORDER_SIDE)*2)/2;
            y_gap=(600-(BORDER_TOP*2+400+BORDER_BOTTOM*2))/2;
#if !defined(SSE_VID_D3D)
          }else if (draw_fs_topgap){
            y_gap=draw_fs_topgap;
#endif//#if !defined(SSE_VID_D3D)
          }
        }
#endif//#if !defined(SSE_VID_D3D)
        HBRUSH br=(HBRUSH)GetStockObject(BLACK_BRUSH);
        RECT rc;
        if (x_gap){
          rc.top=menu_bottom;rc.left=0;rc.bottom=Height;rc.right=x_gap;
          FillRect(ps.hdc,&rc,br);
          rc.left=dest.right-x_gap;rc.right=dest.right;
          FillRect(ps.hdc,&rc,br);
        }
        if (y_gap){
          rc.top=menu_bottom;rc.left=0;rc.bottom=y_gap;rc.right=dest.right;
          FillRect(ps.hdc,&rc,br);
          rc.top=Height-y_gap;rc.bottom=Height;
          FillRect(ps.hdc,&rc,br);
        }
        draw_grille_black=50;
      }else{
        dest.top+=MENUHEIGHT;
        dest.bottom=Height;
        DrawEdge(ps.hdc,&dest,EDGE_SUNKEN,BF_RECT);

        draw_end();
        if (draw_blit()==0){
          dest.left+=2;dest.top+=2;dest.right-=2;dest.bottom-=2;
          FillRect(ps.hdc,&dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
        }
      }
      EndPaint(Win,&ps);
      return 0;
    }
    case WM_USER+2:   // Update commands
#if defined(SSE_VAR_NO_UPDATE)
      if (wPar==2323)  // Running?
        return runstate==RUNSTATE_RUNNING;
#else
      if (wPar==54542){       // New Steem
        UpdateWin=(HWND)lPar;
        ShowWindow(GetDlgItem(Win,120),int(UpdateWin ? SW_SHOW:SW_HIDE));
        if (runstate==RUNSTATE_RUNNING && UpdateWin){
          osd_start_scroller(EasyStr(T("Steem update! Steem version "))+
                              GetCSFStr("Update","LatestVersion","1.3",INIFile)+" "+
                              T("is ready to be downloaded. Click on the new button in the toolbar (to the right of paste) for more details."));
        }
        return 0;
      }else if (wPar==2323){  // Running?
        return runstate==RUNSTATE_RUNNING;
      }else if (wPar==12345){ // New Patches
        if (PatchesBox.IsVisible()){
          PatchesBox.RefreshPatchList();
        }else{
          PatchesBox.SetButtonIcon();
        }
        return 0;
      }
#endif
      break;
    case WM_COMMAND:
      if (LOWORD(wPar)>=100 && LOWORD(wPar)<200){
        int NotifyMess=HIWORD(wPar);
        if (NotifyMess==BN_CLICKED){
          HandleButtonMessage(LOWORD(wPar),HWND(lPar));
        }else if (LOWORD(wPar)==109){
          if (NotifyMess==BN_PUSHED || NotifyMess==BN_UNPUSHED || NotifyMess==BN_DBLCLK){
            if (NotifyMess==BN_DBLCLK) fast_forward_stuck_down=true;
            if (fast_forward_stuck_down){
              if (NotifyMess==BN_UNPUSHED) break;
              if (NotifyMess==BN_PUSHED) NotifyMess=BN_UNPUSHED;  // Click to turn off
            }
            fast_forward_change(NotifyMess!=BN_UNPUSHED,SendMessage(HWND(lPar),BM_GETCLICKBUTTON,0,0)==2);
          }
        }else if (LOWORD(wPar)==101){
          if (NotifyMess==BN_PUSHED || NotifyMess==BN_UNPUSHED){
            if (SendMessage(HWND(lPar),BM_GETCLICKBUTTON,0,0)==2 || NotifyMess==BN_UNPUSHED){
              slow_motion_change(NotifyMess==BN_PUSHED);
            }
          }
        }
      }else if ((LOWORD(wPar)>=210 && LOWORD(wPar)<220) || LOWORD(wPar)==203 || LOWORD(wPar)==207 || LOWORD(wPar)==208
#if defined(SSE_GUI_SNAPSHOT_INI)
        || LOWORD(wPar)==209
#endif
        ){
        if (runstate==RUNSTATE_STOPPED){
          bool AddToHistory=true;
          if (LOWORD(wPar)>=210) LastSnapShot=StateHist[LOWORD(wPar)-210];
          EasyStr fn=LastSnapShot;
          if (LOWORD(wPar)==207) fn=WriteDir+SLASH+"auto_reset_backup.sts", AddToHistory=0;
          if (LOWORD(wPar)==208) fn=WriteDir+SLASH+"auto_loadsnapshot_backup.sts", AddToHistory=0;
#if defined(SSE_GUI_SNAPSHOT_INI)
          if(LOWORD(wPar)==209)
            fn=DefaultSnapshotFile;
#endif
          LoadSnapShot(fn,AddToHistory);
          if (LOWORD(wPar)==207 || LOWORD(wPar)==208) DeleteFile(fn);
        }else{
          runstate=RUNSTATE_STOPPING;
          PostMessage(Win,Mess,wPar,lPar); // Keep delaying message until stopped
          return 0;
        }
      }else if (LOWORD(wPar)>=300 && LOWORD(wPar)<311){
        PasteSpeed=LOWORD(wPar)-299;
      }else if (LOWORD(wPar)>=400 && LOWORD(wPar)<450){
        if (LOWORD(wPar)<420){ // Change screenshot format
          EasyStringList format_sl;
          Disp.ScreenShotGetFormats(&format_sl);
          OptionBox.ChangeScreenShotFormat(format_sl[LOWORD(wPar)-400].Data[0],format_sl[LOWORD(wPar)-400].String);
        }else if (LOWORD(wPar)<440){ // Change screenshot format options
          EasyStringList format_sl;
#if !defined(SSE_VID_D3D_NO_FREEIMAGE)
          Disp.ScreenShotGetFormatOpts(&format_sl);
          OptionBox.ChangeScreenShotFormatOpts(format_sl[LOWORD(wPar)-420].Data[0]);
#endif
        }else if (LOWORD(wPar)==440){ // Change folder
          OptionBox.ChooseScreenShotFolder(Win);
        }else if (LOWORD(wPar)==441){ // Open folder
          ShellExecute(NULL,NULL,ScreenShotFol,"","",SW_SHOWNORMAL);
        }else if (LOWORD(wPar)==442){ // Minimum size shots
          Disp.ScreenShotMinSize=!Disp.ScreenShotMinSize;
          if (OptionBox.Handle){
            if (GetDlgItem(OptionBox.Handle,1024)){
              SendMessage(GetDlgItem(OptionBox.Handle,1024),BM_SETCHECK,Disp.ScreenShotMinSize,0);
            }
          }
        }
#if defined(SSE_GUI_CONFIG_FILE)
/*  v3.8.0 Player has clicked on the 'Configuration' icon, then
    on 'Load configuration file' or 'Save configuration file'.
    We load/save an ini file like it was steem.ini, using Steem's
    powerful system (thx Steem authors), which minimises code and
    will satisfy most players.
*/
        else if(LOWORD(wPar)==443 || LOWORD(wPar)==444)
        {
          Str LastCfgFol=LastCfgFile;
          RemoveFileNameFromPath(LastCfgFol,REMOVE_SLASH);
          EasyStr FilNam=FileSelect(Win,LOWORD(wPar)==443
            ?T("Load configuration file"):T("Save configuration file"),
            LastCfgFol,
#if defined(SSE_VS2015)
			FSTypes(0, T("Configuration files").Text, "*." CONFIG_FILE_EXT, NULL),
#else
            FSTypes(0,T("Configuration files").Text,"*."CONFIG_FILE_EXT,NULL),
#endif
            1,(LOWORD(wPar)==443),CONFIG_FILE_EXT,
            GetFileNameFromPath(LastCfgFile));
          if (FilNam.NotEmpty()){
            LastCfgFile=FilNam;
            ConfigStoreFile CSF; //on the stack
            CSF.Open(FilNam);
            // Load
            if (LOWORD(wPar)==443){
              OPTION_WS=CSF.GetInt("Machine","WakeUpState",0);
              LoadAllDialogData(false,"",NULL,&CSF); // radical!
              ROMFile=CSF.GetStr("Machine","ROM_File",ROMFile);
#if defined(SSE_GUI_CONFIG_FILE2)
              // add current TOS path if necessary
              if(strchr(ROMFile.Text,SLASHCHAR)==NULL) // no slash = no path
              {
                EasyStr tmp=OptionBox.TOSBrowseDir + SLASH + ROMFile;
                ROMFile=tmp;
              }
              OptionBox.NewROMFile=ROMFile;
#endif
              reset_st(RESET_COLD|RESET_STOP|RESET_CHANGESETTINGS|RESET_BACKUP);
              SetForegroundWindow(StemWin);
            }
            // Save
            else
            {
              SaveAllDialogData(false,"",&CSF); // radical!
              CSF.SetStr("Machine","WakeUpState",EasyStr(OPTION_WS));
            }     
            CSF.Close();
          }
        }
#endif//SSE_GUI_CONFIG_FILE
      }else{
        if (HIWORD(wPar)==0){
          switch (LOWORD(wPar)){
            case 200:       //Load SnapShot
            case 201:       //Save SnapShot
            {
              EnableAllWindows(0,Win);
#ifdef SSE_SOUND_16BIT_CENTRED
              Sound_Stop();
#else
              Sound_Stop(0);
#endif
              int old_runstate=runstate;
              if (FullScreen && runstate==RUNSTATE_RUNNING){
                runstate=RUNSTATE_STOPPED;
#if defined(SSE_VS2008_WARNING_390)
                Disp.RunEnd();
#else
                Disp.RunEnd(0);
#endif
                UpdateWindow(StemWin);
              }

              EasyStr FilNam;
              Str LastStateFol=LastSnapShot;
              RemoveFileNameFromPath(LastStateFol,REMOVE_SLASH);
              if (LOWORD(wPar)==200){
                FilNam=FileSelect(Win,T("Load Memory Snapshot"),LastStateFol,
                                          FSTypes(0,T("Steem Memory Snapshots").Text,"*.sts",NULL),
                                          1,true,"sts",GetFileNameFromPath(LastSnapShot));
              }else if (LOWORD(wPar)==201){
                FilNam=FileSelect(Win,T("Save Memory Snapshot"),LastStateFol,
                                          FSTypes(0,T("Steem Memory Snapshots").Text,"*.sts",NULL),
                                          1,0,"sts",GetFileNameFromPath(LastSnapShot));
              }
              if (FilNam.NotEmpty()){
                if (SnapShotGetLastBackupPath().NotEmpty()){
                  DeleteFile(SnapShotGetLastBackupPath());
                }
                LastSnapShot=FilNam;
                if (LOWORD(wPar)==200){
                  if (old_runstate==RUNSTATE_STOPPED){
                    LoadSnapShot(LastSnapShot);
                  }else{
                    old_runstate=RUNSTATE_STOPPING;
                    PostMessage(Win,WM_COMMAND,203,lPar); // Delay load until stopped
                  }
                }else{
                  SaveSnapShot(LastSnapShot,-1);
                }
              }
              SetForegroundWindow(Win);

              runstate=old_runstate;
              if (FullScreen && runstate==RUNSTATE_RUNNING){
                Disp.RunStart(0);
              }
              timer=timeGetTime();
              avg_frame_time_timer=timer;
              avg_frame_time_counter=0;
              auto_frameskip_target_time=timer;
              Sound_Start();

              EnableAllWindows(true,Win);
              break;
            }
            case 205:
              if (SnapShotGetLastBackupPath().NotEmpty()){
                DeleteFile(SnapShotGetLastBackupPath());
                MoveFile(LastSnapShot,SnapShotGetLastBackupPath()); // Make backup
              }
              SaveSnapShot(LastSnapShot,-1);
              break;
            case 206:
              // Restore backup, can only get here if backup path is valid
              DeleteFile(LastSnapShot);
              MoveFile(SnapShotGetLastBackupPath(),LastSnapShot);
              break;
          }
        }
      }
      break;
    case WM_USER:
      if (wPar==1234){
#ifndef ONEGAME
        SendMessage(GetDlgItem(Win,100),BM_SETCHECK,DiskMan.IsVisible(),0);
        SendMessage(GetDlgItem(Win,103),BM_SETCHECK,JoyConfig.IsVisible(),0);
        SendMessage(GetDlgItem(Win,105),BM_SETCHECK,InfoBox.IsVisible(),0);
        SendMessage(GetDlgItem(Win,107),BM_SETCHECK,OptionBox.IsVisible(),0);
        SendMessage(GetDlgItem(Win,112),BM_SETCHECK,ShortcutBox.IsVisible(),0);
        SendMessage(GetDlgItem(Win,113),BM_SETCHECK,PatchesBox.IsVisible(),0);
#endif
      }else if (wPar==12345){
        if (DisableFocusWin) SetForegroundWindow(DisableFocusWin);
      }else if (wPar==123){
        // Allows external programs to press ST keys
        WORD VKCode=LOWORD(lPar);
        if (VKCode==VK_LCONTROL || VKCode==VK_RCONTROL) VKCode=VK_CONTROL;
        if (VKCode==VK_LMENU || VKCode==VK_RMENU) VKCode=VK_MENU;
        int ChangeModMask=0;
        if (VKCode==VK_SHIFT)   ChangeModMask=b00000011;
        if (VKCode==VK_LSHIFT)  ChangeModMask=b00000001;
        if (VKCode==VK_RSHIFT)  ChangeModMask=b00000010;
        if (VKCode==VK_CONTROL) ChangeModMask=b00001100;
        if (VKCode==VK_MENU)    ChangeModMask=b00110000;
        if (HIWORD(lPar)){
          ExternalModDown&=~ChangeModMask;
        }else{
          ExternalModDown|=ChangeModMask;
        }
        if (VKCode==VK_SHIFT){
          if (ST_Key_Down[VK_LSHIFT] != !HIWORD(lPar)) HandleKeyPress(VK_LSHIFT,HIWORD(lPar),IGNORE_EXTEND | NO_SHIFT_SWITCH);
          if (ST_Key_Down[VK_RSHIFT] != !HIWORD(lPar)) HandleKeyPress(VK_RSHIFT,HIWORD(lPar),IGNORE_EXTEND | NO_SHIFT_SWITCH);
        }else{
          HandleKeyPress(VKCode,HIWORD(lPar),IGNORE_EXTEND | NO_SHIFT_SWITCH);
        }

      }else if (wPar==12){
        // Return from fullscreen
        SetWindowLong(StemWin,GWL_STYLE,WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE);
        if ((GetWindowLong(StemWin,GWL_STYLE) & WS_SIZEBOX)==0 && timeGetTime()<Disp.ChangeToWinTimeOut){
          PostMessage(StemWin,WM_USER,12,0);
          break;
        }
        SetWindowLong(DiskMan.Handle,GWL_STYLE,(GetWindowLong(DiskMan.Handle,GWL_STYLE) & ~WS_MAXIMIZE) | WS_MINIMIZEBOX);
        bool MaximizeDiskMan=DiskMan.Maximized && DiskMan.IsVisible();
        for (int n=0;n<nStemDialogs;n++){
          DEBUG_ONLY( if (DialogList[n]!=&HistList) ) DialogList[n]->MakeParent(NULL);
        }

        SetParent(ToolTip,NULL);
        if (Disp.BorderPossible()==0){
          border=0;
          OptionBox.EnableBorderOptions(0);
        }

        ShowWindow(GetDlgItem(StemWin,106),SW_HIDE);
#if defined(SSE_VID_FS_PROPER_QUIT_BUTTON)
        ShowWindow(GetDlgItem(StemWin,116),SW_HIDE);
#endif

        if (MaximizeDiskMan) ShowWindow(DiskMan.Handle,SW_MAXIMIZE);

        SendMessage(StemWin,WM_USER,13,0);
      }else if (wPar==13){ // Return from fullscreen
        SetWindowPos(StemWin,HWND(bAOT ? HWND_TOPMOST:HWND_NOTOPMOST),rcPreFS.left,rcPreFS.top,rcPreFS.right-rcPreFS.left,rcPreFS.bottom-rcPreFS.top,0);
        UpdateWindow(StemWin);
        RECT rc;
        GetWindowRect(StemWin,&rc);
        if (EqualRect(&rc,&rcPreFS)==0 && timeGetTime()<Disp.ChangeToWinTimeOut){
          PostMessage(StemWin,WM_USER,13,0);
          break;
        }
        SetWindowLong(StemWin,GWL_STYLE,WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE);
        SetForegroundWindow(StemWin);
        CheckResetDisplay();

        palette_convert_all();
        draw(true);

        if (Disp.RunOnChangeToWindow){
          PostRunMessage();
          Disp.RunOnChangeToWindow=0;
        }
      }
      break;
    case WM_NCLBUTTONDBLCLK:
      if (wPar==HTCAPTION){
        PostMessage(Win,WM_SYSCOMMAND,WPARAM(IsZoomed(Win) ? SC_RESTORE:SC_MAXIMIZE),0);
        return 0;
      }
      break;
    case WM_SYSCOMMAND:
      if (wPar>=100 && wPar<110 && wPar!=102){
        if (IsZoomed(Win)) ShowWindow(Win,SW_SHOWNORMAL);
      }
      switch (wPar){
        case 101: //1:1
          StemWinResize();
          return 0;
        case 103: //Aspect Ratio
        {
          RECT rc;
          GetClientRect(Win,&rc);
          double ratio;
          int res=int(mixed_output ? 1:screen_res);
          int Idx=WinSizeForRes[res];
#if defined(SSE_VID_BORDERS_GUI_392)
          if (border){
#else
          if (border & 1){
#endif
            ratio=(double)(WinSizeBorder[res][Idx].x)/(double)(WinSizeBorder[res][Idx].y);
          }else{
            ratio=(double)(WinSize[res][Idx].x)/(double)(WinSize[res][Idx].y);
          }
          double sz=( (double)(rc.right-4)/ratio + (double)(rc.bottom-(MENUHEIGHT+4)))/2.0;
          SetStemWinSize((int)(sz*ratio + 0.5),(int)(sz + 0.5));

          return 0;
        }
        case 102:
          bAOT=!bAOT;
          CheckMenuItem(StemWin_SysMenu,102,MF_BYCOMMAND | int(bAOT ? MF_CHECKED:MF_UNCHECKED));
          SetWindowPos(StemWin,HWND(bAOT ? HWND_TOPMOST:HWND_NOTOPMOST),0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
          return 0;
        case 104: //bigger window
        case 105: //smaller window
          if(ResChangeResize){
            int res=int(mixed_output ? 1:screen_res);
            int size=WinSizeForRes[res];
            if (wPar==104){
              if(size<3){
                size++;
              }
            }else{
              if(size>0){
                size--;
              }
            }
            WinSizeForRes[res]=size;
            StemWinResize();
            OptionBox.UpdateWindowSizeAndBorder();
          }else{
            RECT rc;
            GetClientRect(StemWin,&rc);
            if (wPar==104){
              rc.right=rc.right*14142/10000;
              rc.bottom=rc.bottom*14142/10000;
            }else{
              rc.right=rc.right*10000/14142;
              rc.bottom=rc.bottom*10000/14142;
            }
            SetStemWinSize(rc.right,rc.bottom);
          }
          return 0;
        case 110:case 111:case 112: //Borders
#if defined(SSE_VID_DISABLE_AUTOBORDER) && defined(SSE_VS2008_WARNING_390) \
  && !defined(SSE_VID_BORDERS_GUI_392)
          OptionBox.SetBorder((wPar-110)!=0);
#else
          OptionBox.SetBorder(wPar-110);
#endif
          OptionBox.UpdateWindowSizeAndBorder();
#if defined(SSE_VID_DISABLE_AUTOBORDER)
          CheckMenuRadioItem(StemWin_SysMenu,110,112,110+min((int)border,1),MF_BYCOMMAND);
#else
          CheckMenuRadioItem(StemWin_SysMenu,110,112,110+min(border,2),MF_BYCOMMAND);
#endif
          return 0;
        case 113:
          OptionBox.ChangeOSDDisable(!osd_disable);
          return 0;
      }
      switch (wPar & 0xFFF0){
        case SC_MAXIMIZE:
          if (Disp.CanGoToFullScreen()){
            Disp.ChangeToFullScreen();
            return 0;
          }
          break;
        case SC_MONITORPOWER:
          if (runstate == RUNSTATE_RUNNING) return 0;
          break;
        case SC_SCREENSAVE:
          //SS this prevents screensaver from activating but only if Steem has 
          // focus, else we don't get this message
          if (runstate == RUNSTATE_RUNNING || FullScreen) return 0;
          break;
        case SC_TASKLIST:case SC_PREVWINDOW:case SC_NEXTWINDOW:
          if (runstate==RUNSTATE_RUNNING) return 0;
          break;
      }
      break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:case WM_SYSKEYUP:
      if (bAppActive==0) return 0;
      if (wPar==VK_SHIFT || wPar==VK_CONTROL || wPar==VK_MENU) return 0;
#ifndef ONEGAME
      if (TaskSwitchDisabled){
        int n=0;
        while (TaskSwitchVKList[n]) if (LOBYTE(wPar)==TaskSwitchVKList[n++]) return 0;
      }
#endif
      
#if defined(SSE_GUI_F12)
      // Adding F12 as emulator start/stop.
      if(wPar==VK_F12)
      {
        if (Mess==WM_KEYUP || Mess==WM_SYSKEYUP)
        {
          if(runstate==RUNSTATE_STOPPED)
          { PostRunMessage();} // it's a macro
          else
          {
#if defined(SSE_BOILER_FRAME_REPORT) && defined(SSE_BOILER_FRAME_REPORT_ON_STOP)
            FrameEvents.Report();
#endif

            runstate=RUNSTATE_STOPPING;
          }
        }
      }
#endif

      if (runstate==RUNSTATE_RUNNING){
#ifndef ONEGAME
        if (wPar==VK_PAUSE){
          if (Mess==WM_KEYUP || Mess==WM_SYSKEYUP){
            if (FullScreen || GetKeyState(VK_SHIFT)<0){
#if defined(SSE_BOILER_FRAME_REPORT) && defined(SSE_BOILER_FRAME_REPORT_ON_STOP)
              FrameEvents.Report();
#endif
              runstate=RUNSTATE_STOPPING;
            }else{
              if (stem_mousemode==STEM_MOUSEMODE_DISABLED){
                SetForegroundWindow(StemWin);
                SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
              }else{
                SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
              }
            }
          }
#else
        if (wPar==VK_PAUSE || wPar==VK_F12 || wPar==VK_ESCAPE){
          if (Mess==WM_KEYUP || Mess==WM_SYSKEYUP){
            if (runstate==RUNSTATE_RUNNING){
              OGStopAction=OG_QUIT;
              runstate=RUNSTATE_STOPPING;
            }
          }
#endif
        }else if (Mess==WM_KEYUP || Mess==WM_SYSKEYUP || (lPar & 0x40000000)==0){
          GetRealVKCodeForKeypad(wPar,lPar);
          bool Extended=(lPar & 0x1000000)!=0;
          if (joy_is_key_used(BYTE(wPar))==0 && CutDisableKey[BYTE(wPar)]==0){
            HandleKeyPress(wPar,Mess==WM_KEYUP || Mess==WM_SYSKEYUP,Extended);
          }
        }
      }else if (runstate==RUNSTATE_STOPPED){
        if (Mess==WM_SYSKEYUP && wPar==VK_RETURN &&
            GetForegroundWindow()==Win && GetAsyncKeyState(VK_MENU)<0){
          if (FullScreen){
            Disp.ChangeToWindowedMode();
          }else{
            if (Disp.CanGoToFullScreen()) Disp.ChangeToFullScreen();
          }
        }else if ((Mess==WM_KEYUP || Mess==WM_SYSKEYUP) && (wPar==VK_PAUSE || wPar==VK_CANCEL)){
          PostRunMessage();
        }else if ((Mess==WM_SYSKEYDOWN && wPar==VK_F4) ||
                  (Mess==WM_KEYDOWN && wPar=='W' && GetKeyState(VK_CONTROL)<0)){
          PostMessage(Win,WM_CLOSE,0,0);
        }
      }
      return 0;
    case WM_SYSCHAR:case WM_SYSDEADCHAR:
      return 0;
    case WM_LBUTTONDOWN:case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:case WM_RBUTTONUP:
#ifdef DEBUG_BUILD
      if (runstate==RUNSTATE_STOPPED && stem_mousemode==STEM_MOUSEMODE_BREAKPOINT){
        if (wPar & MK_LBUTTON){
          int x=LOWORD(lPar)-2,y=HIWORD(lPar)-2-MENUHEIGHT;
          x&=0xfffffff0; //16 pixels per raster
          x/=16;         //to raster number
          MEM_ADDRESS ad=xbios2;
          if (screen_res==2) ad+=y*80;else ad+=y*160;
          if (screen_res==0) ad+=x*8;else if(screen_res==1)ad+=x*4;else ad+=x*2;
          d2_dpoke(ad,0xface);
          debug_set_mon(ad,0,0xffff);
          SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
        }
      }
#endif
      if (HIWORD(lPar)>MENUHEIGHT && stem_mousemode==STEM_MOUSEMODE_DISABLED){
        if (GetForegroundWindow()==StemWin){
          if (runstate==RUNSTATE_RUNNING){
            SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
          }else if (StartEmuOnClick){
            PostRunMessage();
          }
#if defined(SSE_BOILER_REPORT_SCAN_Y_ON_CLICK)
/*  When emulation is stopped, right click in window will tell which
    scanline we're at.
    To do this we use the "status bar".
    We assume double height, we don't compute this.
    While we're at it, we also report guessed X
*/
          else if(Mess==WM_RBUTTONDOWN)
          {
            int guessed_scan_y=(HIWORD(lPar)-MENUHEIGHT)-1;
            if(screen_res<2)
              guessed_scan_y/=2;
            if(border)
              guessed_scan_y-=(screen_res==2)?BORDER_TOP*2:BORDER_TOP;
#if defined(SSE_VID_BORDERS)
            int guessed_x=LOWORD(lPar)/2-SideBorderSizeWin;
#else
            int guessed_x=LOWORD(lPar)/2;
#endif
#if defined(SSE_GUI_STATUS_BAR)
            
#if defined(SSE_GUI_STATUS_BAR_ICONS)
#if defined(SSE_BOILER_REPORT_SDP_ON_CLICK) && defined(SSE_SHIFTER)
            MEM_ADDRESS computed_sdp=FrameEvents.GetSDP(guessed_x,guessed_scan_y);
            sprintf(ansi_string,"X%d Y%d $%X",guessed_x,guessed_scan_y,computed_sdp);
#else
            sprintf(ansi_string,"X%d Y%d",guessed_x,guessed_scan_y);
#endif
            M68000.ProcessingState=TM68000::BOILER_MESSAGE;
            HWND status_bar_win=GetDlgItem(StemWin,120); // get handle
            InvalidateRect(status_bar_win,NULL,false);
#else
            HWND status_bar_win=GetDlgItem(StemWin,120); // get handle
#if defined(SSE_BOILER_REPORT_SDP_ON_CLICK) && defined(SSE_SHIFTER)
            char tmp[12+1+6];
#if defined(SSE_BOILER_FRAME_REPORT)
            MEM_ADDRESS computed_sdp = FrameEvents.GetSDP(guessed_x,guessed_scan_y);
            sprintf(tmp,"X%d Y%d $%X",guessed_x,guessed_scan_y,computed_sdp);
#endif
#else // 1st, only X & Y
            char tmp[12];
            sprintf(tmp,"X%d Y%d",guessed_x,guessed_scan_y);
#endif
            SendMessage(status_bar_win,WM_SETTEXT,0,(LPARAM)(LPCTSTR)tmp);
#endif//#define SSE_GUI_STATUS_BAR_ICONS
#endif
          }

#endif
        }
      }
      break;
    case WM_MOUSEWHEEL:
      MouseWheelMove+=short(HIWORD(wPar));
      return 0;
    case WM_TIMER:
      if (wPar==SHORTCUTS_TIMER_ID){
        if (bAppActive) JoyGetPoses();
        ShortcutsCheck();
      }else if (wPar==DISPLAYCHANGE_TIMER_ID){
        KillTimer(Win,DISPLAYCHANGE_TIMER_ID);
        ConfigStoreFile CSF(INIFile);
        LoadAllIcons(&CSF,0);
        CSF.Close();
      }
      break;
    case WM_COPYDATA:
    case WM_DROPFILES:
    {
      EasyStr *Files=NULL;
      char **lpFile;
      int nFiles;
      if (Mess==WM_COPYDATA){
        COPYDATASTRUCT *cds=(COPYDATASTRUCT*)lPar;
        if (cds->dwData!=MAKECHARCONST('S','C','O','M')) break;  // Not Steem comline file
        nFiles=1;
        lpFile=(char**)&(cds->lpData); // lpFile is an array of nFiles pointers to char*s, cds->lpData is a char*
      }else{
        nFiles=DragQueryFile((HDROP)wPar,0xffffffff,NULL,0);
        Files=new EasyStr[nFiles];
        lpFile=new char*[nFiles];
        for (int i=0;i<nFiles;i++){
          Files[i].SetLength(MAX_PATH);
          DragQueryFile((HDROP)wPar,i,Files[i],MAX_PATH);
          lpFile[i]=Files[i].Text;
        }
        DragFinish((HDROP)wPar);
      }
      EasyStr OldDiskA=FloppyDrive[0].GetDisk();

      BootStateFile="";BootTOSImage=0;
      BootDisk[0]="";BootDisk[1]="";
      BootInMode=0;
      ParseCommandLine(nFiles,lpFile);

      if (BootStateFile.NotEmpty()){
        if (LoadSnapShot(BootStateFile)){
          SetForegroundWindow(Win);
          PostRunMessage();
        }
      }else{
        for (int Drive=0;Drive<2;Drive++){
          if (BootDisk[Drive].NotEmpty()){
            EasyStr Name=GetFileNameFromPath(BootDisk[Drive]);
            *strrchr(Name,'.')=0;
            DiskMan.InsertDisk(Drive,Name,BootDisk[Drive],0,0);
          }
        }
        bool ChangedDisk=NotSameStr_I(OldDiskA,FloppyDrive[0].GetDisk());
        if (BootTOSImage || ChangedDisk){
          SetForegroundWindow(Win);
          reset_st(RESET_COLD | DWORD(ChangedDisk ? RESET_NOSTOP:RESET_STOP) | RESET_CHANGESETTINGS | RESET_BACKUP);
          if (ChangedDisk && runstate!=RUNSTATE_RUNNING) PostRunMessage();
        }else if (BootInMode & BOOT_MODE_RUN){
          if (runstate==RUNSTATE_STOPPED) PostRunMessage();
        }
      }
      if (Mess==WM_COPYDATA) return MAKECHARCONST('Y','A','Y','S');

      delete[] Files;
      delete[] lpFile;
      return 0;
    }
    case WM_GETMINMAXINFO:
      ((MINMAXINFO*)lPar)->ptMinTrackSize.x=320+GetSystemMetrics(SM_CXFRAME)*2+4;
      ((MINMAXINFO*)lPar)->ptMinTrackSize.y=200+GetSystemMetrics(SM_CYFRAME)*2+GetSystemMetrics(SM_CYCAPTION)+MENUHEIGHT+4;
      break;
#if defined(SSE_VID_BLOCK_WINDOW_SIZE)
/*  v3.7 
    Prevent player from resizing the window by dragging the border.
    Optional because stretching is cool and handy too.
    We pretend the mouse is on the client area, so the resizing cursor 
    won't even appear.
    All border values are between HTLEFT and HTBOTTOMRIGHT.
    Returning HTCLIENT all the time would work with Windows 7 but not Vista 
    (can't move or close window).
*/
    case WM_NCHITTEST:
    {
      DWORD val=DefWindowProc(Win,Mess,wPar,lPar); // real area
      if(OPTION_BLOCK_RESIZE && val>=HTLEFT && val <=HTBOTTOMRIGHT)
        val=HTCLIENT;
      return val;
    }
#endif

#if defined(SSE_VID_LOCK_ASPET_RATIO)
/*  v3.7
    if option above isn't checked, this one enforces a +- correct aspect ratio
    lPar points to the absolute resizing rectangle, its values may be changed
    GetWindowRect() gives the current rectangle of the window, hopefully the
    same concept.
    TODO: keep AR really constant (computing may produce deviation)
*/
    case WM_SIZING:
      if(OPTION_LOCK_ASPECT_RATIO)
      {
        RECT current_coord;
        GetWindowRect(StemWin,&current_coord);
        float a_r=(float)(current_coord.right-current_coord.left)/(float)(current_coord.bottom-current_coord.top);
        if(a_r)
        {
          if(((RECT*)lPar)->right>current_coord.right)
            ((RECT*)lPar)->bottom= (float)(((RECT*)lPar)->right-((RECT*)lPar)->left)/a_r + ((RECT*)lPar)->top;
          else
            ((RECT*)lPar)->right= (float)(((RECT*)lPar)->bottom-((RECT*)lPar)->top)*a_r + ((RECT*)lPar)->left;
        }
      }
      break;
#endif

    case WM_SIZE:
    {
      int cw=LOWORD(lPar),ch=HIWORD(lPar);
      RECT rc={0,MENUHEIGHT,cw,ch};
      //TRACE("WM_SIZE ");TRACE_RECT(rc);
      InvalidateRect(Win,&rc,0);
#ifndef ONEGAME
      if (FullScreen){
#if defined(SSE_VID_FS_PROPER_QUIT_BUTTON)
        SetWindowPos(GetDlgItem(Win,106),0,cw-43,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);//SS backs to windowed mode
        SetWindowPos(GetDlgItem(Win,116),0,cw-20,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);//SS backs to windowed mode
        cw-=50;
#else
        if (FSQuitBut) SetWindowPos(FSQuitBut,0,0,ch-14-MENUHEIGHT,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(Win,106),0,cw-20,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);//SS backs to windowed mode
        cw-=30;
#endif
        
      }
#if defined(SSE_VAR_OPT_382)//useless?
      UINT mask=(SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
      SetWindowPos(GetDlgItem(Win,105),0,cw-135,0,0,0,mask);
      SetWindowPos(GetDlgItem(Win,113),0,cw-112,0,0,0,mask);
      SetWindowPos(GetDlgItem(Win,112),0,cw-89,0,0,0,mask);
      SetWindowPos(GetDlgItem(Win,107),0,cw-66,0,0,0,mask);
      SetWindowPos(GetDlgItem(Win,103),0,cw-43,0,0,0,mask);
      SetWindowPos(GetDlgItem(Win,100),0,cw-20,0,0,0,mask);
#else
      SetWindowPos(GetDlgItem(Win,105),0,cw-135,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
      SetWindowPos(GetDlgItem(Win,113),0,cw-112,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
      SetWindowPos(GetDlgItem(Win,112),0,cw-89,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
      SetWindowPos(GetDlgItem(Win,107),0,cw-66,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
      SetWindowPos(GetDlgItem(Win,103),0,cw-43,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
      SetWindowPos(GetDlgItem(Win,100),0,cw-20,0,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
#endif
      if (ResetInfoWin) SendMessage(ResetInfoWin,WM_USER,1789,0);
#endif

      if (draw_grille_black<4) draw_grille_black=4;
      if (FullScreen){
        CanUse_400=true;

#if defined(SSE_VID_BORDERS)
      }
#if defined(SSE_VID_BORDERS_GUI_392)
      else if (border)
#else
      else if (border & 1)
#endif
      {// blurry display in 'no stretch' mode? check here!!!!!!!!!!!!!!!!
        CanUse_400=
#if defined(SSE_VID_BORDERS_413)
          (
          (cw==4+640+4*SideBorderSizeWin) 
          ||((SideBorderSizeWin==VERY_LARGE_BORDER_SIDE_WIN)&&(cw-2==4+640+4*SideBorderSizeWin)
          )
#else
          (cw==(4+640+ 4* SideBorderSizeWin) 
#endif
          && ch==(MENUHEIGHT+4+400 + 2*(BORDER_TOP+BottomBorderSize)));
        TRACE_LOG("CanUse_400 %d cw %d %d ch %d %d\n",CanUse_400,cw,(4+640+ 4* SideBorderSizeWin),ch,(MENUHEIGHT+4+400 + 2*(BORDER_TOP+BottomBorderSize)));
      }else{
        CanUse_400=(cw==644 && ch==404+MENUHEIGHT);
        TRACE_LOG("CanUse_400 %d cw %d ch %d\n",CanUse_400,cw,ch);
      }
#else
    }else if (border & 1){
        CanUse_400=(cw==(4+640+16*4*2) && ch==(MENUHEIGHT+4+400 + 2*(BORDER_TOP+BORDER_BOTTOM)));
      }else{
        CanUse_400=(cw==644 && ch==404+MENUHEIGHT);
      }

#endif

      switch (wPar){
        case SIZE_MAXIMIZED: bAppMaximized=true; break;
        case SIZE_MINIMIZED: bAppMinimized=true; break;
        case SIZE_RESTORED:
          if (bAppMinimized){
            bAppMinimized=0;
          }else if (bAppMaximized){
            bAppMaximized=0;
          }
          break;
      }
#if defined(SSE_GUI_STATUS_BAR)
      GUIRefreshStatusBar();//of course (v3.7.0) - there must be other places
#endif
      break;
    }
    case WM_DISPLAYCHANGE:
      if (FullScreen==0){
        bool old_draw_lock=draw_lock;

        OptionBox.EnableBorderOptions(Disp.BorderPossible());
        //TRACE("displaychange ");
        Disp.ScreenChange();
        palette_convert_all();
        draw(false);

        if (old_draw_lock) draw_begin();
      }

      SetTimer(Win,DISPLAYCHANGE_TIMER_ID,500,NULL);

      break;
    case WM_CHANGECBCHAIN:
      if ((HWND)wPar==NextClipboardViewerWin){
        NextClipboardViewerWin=(HWND)lPar;
      }else if (NextClipboardViewerWin){
        SendMessage(NextClipboardViewerWin,Mess,wPar,lPar);
      }
      break;
    case WM_DRAWCLIPBOARD:
#if !defined(SSE_GUI_NO_PASTE)
      UpdatePasteButton();
#endif
      if (NextClipboardViewerWin) SendMessage(NextClipboardViewerWin,Mess,wPar,lPar);
      break;
    case WM_ACTIVATEAPP:
#if defined(SSE_VS2008_WARNING_390)
      bAppActive=(wPar!=0);
#else
      bAppActive=(bool)wPar;
#endif
      if (FullScreen){
        if (wPar){  //Activating
#if !defined(SSE_VID_D3D)
          if (using_res_640_400){
            using_res_640_400=0;
            change_fullscreen_display_mode(true);
          }
#endif
          for (int n=0;n<nStemDialogs;n++){
            if (DialogList[n]->Handle){
              SetWindowPos(DialogList[n]->Handle,NULL,DialogList[n]->FSLeft,DialogList[n]->FSTop,0,0,SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            }
          }
          //TRACE("activate ");
#if !defined(SSE_VID_D3D_FS_392C) //?
          Disp.ScreenChange();
#endif
#ifndef ONEGAME
          draw(true);
#else
          PostRunMessage();
#endif
        }else{
          if (runstate==RUNSTATE_RUNNING) runstate=RUNSTATE_STOPPING;
        }
      }
      if (BytesPerPixel==1){
        if (wPar){
          palette_prepare(true);
          AnimatePalette(winpal,palhalf+10,118,(PALETTEENTRY*)(logpal+palhalf+10));
        }else{
          palette_remove();
        }
      }
      break;
    case WM_SETCURSOR:
      switch (LOWORD(lPar)){
        case HTCLIENT:
        {
          POINT pt;GetCursorPos(&pt);ScreenToClient(Win,&pt);
          RECT rc;GetClientRect(Win,&rc);
          if (pt.x>2 && pt.x<rc.right-2 && pt.y>MENUHEIGHT+1 && pt.y<rc.bottom-2){
            if (stem_mousemode==STEM_MOUSEMODE_WINDOW){
              if (no_set_cursor_pos){
                SetCursor(LoadCursor(NULL,RCNUM(IDC_CROSS)));//SS TODO?
              }else{
                SetCursor(NULL);
              }
            }else if (stem_mousemode==STEM_MOUSEMODE_BREAKPOINT){
              SetCursor(LoadCursor(NULL,IDC_CROSS));
            }else{
              SetCursor(PCArrow);
            }
            return true;
          }
        }
      }
      break;
    case WM_ACTIVATE:
      if (wPar==WA_INACTIVE){
        SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
        UpdateSTKeys();
        EnableTaskSwitch();
      }else{
        if (IsWindowEnabled(Win)==0){
          PostMessage(StemWin,WM_USER,12345,(LPARAM)Win);
        }
        SetFocus(StemWin);
        if (runstate==RUNSTATE_RUNNING && AllowTaskSwitch==0) DisableTaskSwitch();
      }
      break;
    case WM_KILLFOCUS:
      SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      break;
    case WM_CLOSE:
      QuitSteem();
      return false;
    case WM_QUERYENDSESSION:
      QuitSteem();
      return true;
    case WM_DESTROY:
      ChangeClipboardChain(StemWin,NextClipboardViewerWin);
      StemWin=NULL;
      break;
#if defined(SSE_GUI_STATUS_BAR_392)
    case WM_DRAWITEM: // window must draw its own item
      if(wPar==120) // status bar 
      {
#define myHdc ((DRAWITEMSTRUCT*)lPar)->hDC 
#define myRect ((DRAWITEMSTRUCT*)lPar)->rcItem
        ASSERT(OPTION_STATUS_BAR||OPTION_STATUS_BAR_GAME_NAME);
        ASSERT(((DRAWITEMSTRUCT*)lPar)->CtlType==ODT_STATIC);
        ASSERT(myRect.left==0); ASSERT(myRect.top==0); //which I didn't know
        ASSERT(ansi_string);
#if !defined(BCC_BUILD) && !(defined(VC_BUILD) && _MSC_VER<=1200) // old compilers
        ASSERT(strnlen(ansi_string,MAX_PATH)<MAX_PATH);
#endif
        // erase rectangle (different colour for hires)
        FillRect(myHdc,&myRect,((COLOUR_MONITOR)?
          (HBRUSH)(COLOR_MENU+1):(HBRUSH)(COLOR_WINDOWFRAME+1)));
        GUIRefreshStatusBar(false); // make sure the string is correct (again!)
        int nchars=strlen(ansi_string); // safe version?
        SIZE Size;
        VERIFY(GetTextExtentPoint32(myHdc,ansi_string,nchars,&Size)!=0);
        //TRACE_RECT(myRect); TRACE("%d %d\n",Size.cx,Size.cy);
        int left=myRect.right/2-Size.cx/2;
        //TRACE("left %d\n",left);
        // adjust for trailing icons
        if(OPTION_STATUS_BAR)
        {
          left-=9*(
#if defined(SSE_FLOPPY) && !defined(SSE_FLOPPY_ALWAYS_ADAT)
          (ADAT)
#endif
#if !defined(SSE_IKBD_6301_NOT_OPTIONAL)
          +(OPTION_C1)
#endif
#if !defined(SSE_C2_NOT_OPTIONAL)
          +(OPTION_C2)
#endif
          +(OPTION_HACKS)
          +(!HardDiskMan.DisableHardDrives||ACSI_EMU_ON));
        }
        //TRACE("left %d right %d\n",left,left+Size.cx);
        //TextOut(myHdc,0,3,"L",1);
        //TextOut(myHdc,myRect.right-8,3,"R",1);

        // Text
        TextOut(myHdc,left,3,ansi_string,nchars);
        // Icons
        if(OPTION_STATUS_BAR 
#if defined(SSE_GUI_STATUS_BAR_ALERT)
          && M68000.ProcessingState==TM68000::NORMAL
#endif
          ) 
        {
          HDC TempDC=CreateCompatibleDC(myHdc);
          ASSERT(TempDC);
          HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(Inst,"TOSFLAGS"));
          int FlagIdx=OptionBox.TOSLangToFlagIdx((int)ROM_PEEK(0x1D));
          ASSERT(FlagIdx!=-1); // error code
          if (FlagIdx>=0){ 
             ASSERT(FlagIdx<14); 
#if defined(SSE_GUI_OPTIONS_WU)
            BitBlt(myHdc,left+((OPTION_WS&&OPTION_ADVANCED)?58:51),4,RC_FLAG_WIDTH,
              RC_FLAG_HEIGHT,TempDC,FlagIdx*RC_FLAG_WIDTH,0,SRCCOPY);
#else
            BitBlt(myHdc,left+((OPTION_WS)?58-6:51-6),4,RC_FLAG_WIDTH,
              RC_FLAG_HEIGHT,TempDC,FlagIdx*RC_FLAG_WIDTH,0,SRCCOPY);
#endif
          }
          DeleteObject(SelectObject(TempDC,OldBmp));
          DeleteDC(TempDC);
          int myx=left+Size.cx+3;
#if defined(SSE_FLOPPY) && !defined(SSE_FLOPPY_ALWAYS_ADAT)
          if(ADAT)
          {
            DrawIconEx(myHdc,myx,0,
              hGUIIcon[RC_ICO_ACCURATEFDC],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
          if(!HardDiskMan.DisableHardDrives||ACSI_EMU_ON)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_HARDDRIVE16],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#if !defined(SSE_IKBD_6301_NOT_OPTIONAL)
          if(OPTION_C1)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_OPS_C1],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
#if !defined(SSE_C2_NOT_OPTIONAL)
          if(OPTION_C2)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_OPS_C2],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
          if(OPTION_HACKS)
          {
            DrawIconEx(myHdc,myx,2,
              //hGUIIcon[RC_ICO_PATCHES],16,16,0,NULL,DI_NORMAL);
              hGUIIcon[RC_ICO_OPS_HACKS],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
        }
        return TRUE;
#undef myHdc
#undef myRect
      }
      break;


#elif defined(SSE_GUI_STATUS_BAR_ICONS) //v3.8.0
/*  It is cool to add country flag next to TOS in the status bar, so
    let's do some Windows programming. 
*/
    case WM_DRAWITEM: // window must draw its own item
      if(wPar==120) // status bar 
      {
        ASSERT(OPTION_STATUS_BAR||OPTION_STATUS_BAR_GAME_NAME);
        ASSERT(((DRAWITEMSTRUCT*)lPar)->CtlType==ODT_STATIC);
#define myHdc ((DRAWITEMSTRUCT*)lPar)->hDC 
#define myRect ((DRAWITEMSTRUCT*)lPar)->rcItem
        //TRACE_RECT(myRect);
#if !defined(SSE_VS2008_WARNING_390)
        HWND status_bar_win=GetDlgItem(StemWin,wPar); // get handle
#endif
        // erase rectangle (different colour for hires)
         
        FillRect(myHdc,&((DRAWITEMSTRUCT*)lPar)->rcItem,((COLOUR_MONITOR)?
          (HBRUSH)(COLOR_MENU+1):(HBRUSH)(COLOR_WINDOWFRAME+1)));
        GUIRefreshStatusBar(false); // make sure the string is correct
        ASSERT(ansi_string);
#if !defined(BCC_BUILD) && !(defined(VC_BUILD) && _MSC_VER<=1200) // old compilers
        ASSERT(strnlen(ansi_string,MAX_PATH)<MAX_PATH);
#endif
        int nchars=strlen(ansi_string); // safe version?
        SIZE Size;
        GetTextExtentPoint32(myHdc,ansi_string,nchars,&Size);
        
        myRect.left=myRect.right/2-Size.cx/2;
#if defined(SSE_FLOPPY) && !defined(SSE_FLOPPY_ALWAYS_ADAT)
        if(ADAT)
          myRect.left-=8;
#endif
#if defined(SSE_GUI_STATUS_BAR_HD)
        if(!HardDiskMan.DisableHardDrives||ACSI_EMU_ON)
          myRect.left-=8;
#endif
#if !defined(SSE_IKBD_6301_NOT_OPTIONAL)
        if(OPTION_C1)
          myRect.left-=8;
#endif
#if !defined(SSE_C2_NOT_OPTIONAL)
        if(OPTION_C2)
          myRect.left-=8;
#endif
        if(OPTION_HACKS)
          myRect.left-=8;
        TextOut(myHdc,myRect.left,3,ansi_string,nchars);
        if(OPTION_STATUS_BAR && M68000.ProcessingState==TM68000::NORMAL) 
        {
          HDC TempDC=CreateCompatibleDC(myHdc);
          ASSERT(TempDC);
          HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(Inst,"TOSFLAGS"));
          int FlagIdx=OptionBox.TOSLangToFlagIdx((int)ROM_PEEK(0x1D));
          ASSERT(FlagIdx!=-1); // error code
          if (FlagIdx>=0){ 
             ASSERT(FlagIdx<14); 
            BitBlt(myHdc,myRect.left+((OPTION_WS)?58:51),4,RC_FLAG_WIDTH,
              RC_FLAG_HEIGHT,TempDC,FlagIdx*RC_FLAG_WIDTH,0,SRCCOPY);
          }
          DeleteObject(SelectObject(TempDC,OldBmp));
          DeleteDC(TempDC);
          int myx=myRect.left+Size.cx+3;
#if defined(SSE_FLOPPY) && !defined(SSE_FLOPPY_ALWAYS_ADAT)
          if(ADAT)
          {
            DrawIconEx(myHdc,myx,0,
              hGUIIcon[RC_ICO_ACCURATEFDC],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
#if defined(SSE_GUI_STATUS_BAR_HD)
          if(!HardDiskMan.DisableHardDrives||ACSI_EMU_ON)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_HARDDRIVE16],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
#if !defined(SSE_IKBD_6301_NOT_OPTIONAL)
          if(OPTION_C1)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_OPS_C1],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
#if !defined(SSE_C2_NOT_OPTIONAL)
          if(OPTION_C2)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_OPS_C2],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
#endif
          if(OPTION_HACKS)
          {
            DrawIconEx(myHdc,myx,2,
              hGUIIcon[RC_ICO_PATCHES],16,16,0,NULL,DI_NORMAL);
            myx+=19;
          }
        }
        return TRUE;
#undef myHdc
#undef status_bar
#undef myRect
      }
      break;
#endif
#if defined(SSE_VID_D3D_2SCREENS)
/*  The message keeps being sent while the window is being dragged, it's not
    once after it has been moved.
    We check here if the player dragged the main window over to another screen.
*/
    case WM_MOVE:
      POINT myPoint={(short)LOWORD(lPar),(short)HIWORD(lPar)}; //signed 16bit
      // Get Windows handle to monitor. This function requires Windows 2000.
      HMONITOR hCurrentMonitor=MonitorFromPoint(myPoint,MONITOR_DEFAULTTOPRIMARY);
      if(!PtInRect(&Disp.rcMonitor,myPoint)) // player dragged to other monitor
      {
        TRACE_VID_R("Detect change monitor (%d,%d) not in (%d,%d,%d,%d) handle %p\n",
          myPoint.x,myPoint.y,Disp.rcMonitor.left,Disp.rcMonitor.top,
          Disp.rcMonitor.right,Disp.rcMonitor.bottom,hCurrentMonitor);
        Disp.D3DCheckCurrentMonitorConfig(hCurrentMonitor);
      }
      break;
#endif
  }
	return DefWindowProc(Win,Mess,wPar,lPar);
}
//---------------------------------------------------------------------------
void HandleButtonMessage(UINT Id,HWND hBut)
{
  switch (Id){
    case 100:
      DiskMan.ToggleVisible();
      SendMessage(hBut,BM_SETCHECK,DiskMan.IsVisible(),0);
      break;
    case 103:
      JoyConfig.ToggleVisible();
      SendMessage(hBut,BM_SETCHECK,JoyConfig.IsVisible(),0);
      break;
    case 105:
      InfoBox.ToggleVisible();
      SendMessage(hBut,BM_SETCHECK,InfoBox.IsVisible(),0);
      break;
    case 107:
      OptionBox.ToggleVisible();
      SendMessage(hBut,BM_SETCHECK,OptionBox.IsVisible(),0);
      break;
    case 112:
      ShortcutBox.ToggleVisible();
      SendMessage(hBut,BM_SETCHECK,ShortcutBox.IsVisible(),0);
      break;
    case 113:
      PatchesBox.ToggleVisible();
      SendMessage(hBut,BM_SETCHECK,PatchesBox.IsVisible(),0);
      break;

    case 101: // TOGGLE EMULATION START/STOP
      if (SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==2) break;

      RunMessagePosted=0;
      if (runstate==RUNSTATE_STOPPED){
        if (FullScreen && bAppActive==0) return;

        if (GetForegroundWindow()==StemWin && GetCapture()==NULL && IsIconic(StemWin)==0 &&
            fast_forward!=RUNSTATE_STOPPED+1 && slow_motion!=RUNSTATE_STOPPED+1){
#if defined(SSE_GUI_MOUSE_CAPTURE_OPTION)
          if(OPTION_CAPTURE_MOUSE)
#endif
          SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
        }

        SendMessage(hBut,BM_SETCHECK,1,0);
        run();
        SendMessage(hBut,BM_SETCHECK,0,0);
      }else{
        if (runstate==RUNSTATE_RUNNING){
#if defined(SSE_BOILER_FRAME_REPORT) && defined(SSE_BOILER_FRAME_REPORT_ON_STOP)
          FrameEvents.Report();
#endif
          runstate=RUNSTATE_STOPPING;
          SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
        }
      }
      break;
    case 102:
    {
#if defined(SSE_GUI_RESET_BUTTON)
      bool Warm=(SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==1);
#else
      bool Warm=(SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==2);
#endif
#if defined(SSE_GUI_RESET_BUTTON2)
      reset_st(DWORD(Warm ? RESET_WARM:RESET_COLD) | DWORD(RESET_NOSTOP) |
                  RESET_CHANGESETTINGS | RESET_BACKUP);
#else
      reset_st(DWORD(Warm ? RESET_WARM:RESET_COLD) | DWORD(Warm ? RESET_NOSTOP:RESET_STOP) |
                  RESET_CHANGESETTINGS | RESET_BACKUP);
#endif
      break;
    }
    case 106:
#if defined(SSE_GUI_STATUS_BAR_ALERT) && defined(SSE_BUGFIX_392)
      Disp.ChangeToWindowedMode(M68000.ProcessingState==TM68000::BLIT_ERROR);
#else
      Disp.ChangeToWindowedMode();
#endif
      break;
#if !(defined(SSE_VAR_NO_UPDATE))
    case 120:
      if (UpdateWin){
        SendMessage(hBut,BM_SETCHECK,1,0);
        ShowWindow(UpdateWin,SW_SHOW);
        SetForegroundWindow(UpdateWin);
      }
      break;
#endif
#if defined(SSE_GUI_CONFIG_FILE)
/*  Player has clicked on the 'Configuration' icon, this makes a
    popup menu appear, 'Load configuration file' or 'Save configuration file'.
*/
    case 121:
    {
      RECT rc;
      GetWindowRect(hBut,&rc);
      HMENU Pop=CreatePopupMenu();
      AppendMenu(Pop,MF_STRING,443,T("Load configuration file"));
      AppendMenu(Pop,MF_STRING,444,T("Save configuration file"));
      SendMessage(hBut,BM_SETCHECK,1,0);
      TrackPopupMenu(Pop,TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                      rc.left,rc.bottom,0,StemWin,NULL);
      SendMessage(hBut,BM_SETCHECK,0,0);
      DestroyMenu(Pop);	    
      break;
    }
#endif

    case 108:
    {
      EasyStringList sl;
      SnapShotGetOptions(&sl);

      HMENU SnapShotMenu=CreatePopupMenu();
      for (int i=0;i<sl.NumStrings;i++){
        if (IsSameStr(sl[i].String,"-")){
          AppendMenu(SnapShotMenu,MF_SEPARATOR,0,NULL);
        }else{
          AppendMenu(SnapShotMenu,MF_STRING | int(sl[i].Data[1] ? MF_GRAYED:0),sl[i].Data[0],sl[i].String);
        }
      }

      RECT rc;
      GetWindowRect(hBut,&rc);

      SendMessage(hBut,BM_SETCHECK,1,0);
      TrackPopupMenu(SnapShotMenu,TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                      rc.left,rc.bottom,0,StemWin,NULL);
      SendMessage(hBut,BM_SETCHECK,0,0);

      DestroyMenu(SnapShotMenu);
      break;
    }
#if !defined(SSE_GUI_NO_PASTE)
    case 114:
    {
      if (SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==2){
        HMENU Pop=CreatePopupMenu();
        for (int n=0;n<11;n++) AppendMenu(Pop,MF_STRING,300+n,T("Delay")+" - "+n);
        CheckMenuRadioItem(Pop,300,310,299+PasteSpeed,MF_BYCOMMAND);

        RECT rc;
        GetWindowRect(hBut,&rc);

        SendMessage(hBut,BM_SETCHECK,1,0);
        TrackPopupMenu(Pop,TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                        rc.left,rc.bottom,0,StemWin,NULL);
        if (PasteText.Empty()) SendMessage(hBut,BM_SETCHECK,0,0);

        DestroyMenu(Pop);
        break;
      }else{
        PasteIntoSTAction(STPASTE_TOGGLE);
      }
      break;
    }
#endif
    case 115:
    {
      if (SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==2){
        HMENU Pop=CreatePopupMenu();

        EasyStringList format_sl;
        Disp.ScreenShotGetFormats(&format_sl);

        AppendMenu(Pop,MF_STRING,440,T("Change Screenshots Folder"));
        AppendMenu(Pop,MF_STRING,441,T("Open Screenshots Folder"));
        AppendMenu(Pop,MF_STRING | int(Disp.ScreenShotMinSize ? MF_CHECKED:MF_UNCHECKED),
                      442,T("Minimum Size Screenshots"));
        AppendMenu(Pop,MF_SEPARATOR,0,NULL);

        int sel=0;
        for (int n=0;n<format_sl.NumStrings;n++){
          AppendMenu(Pop,MF_STRING,400+n,format_sl[n].String);
          if (format_sl[n].Data[0]==Disp.ScreenShotFormat) sel=400+n;
        }
        CheckMenuRadioItem(Pop,400,400+format_sl.NumStrings,sel,MF_BYCOMMAND);

        format_sl.DeleteAll();
#if !defined(SSE_VID_D3D_NO_FREEIMAGE)
        Disp.ScreenShotGetFormatOpts(&format_sl);
        if (format_sl.NumStrings){
          AppendMenu(Pop,MF_SEPARATOR,0,NULL);
          for (int n=0;n<format_sl.NumStrings;n++){
            AppendMenu(Pop,MF_STRING,420+n,format_sl[n].String);
          }
         
#if defined(SSE_VID_DD_SCREENSHOT_391)
/*  Bug for JPG quality: it's 0x80, 0x100, 0x200, 0x400, 0x800, don't know a
    more trivial way
*/
          CheckMenuRadioItem(Pop,420,420+format_sl.NumStrings,
            ((Disp.ScreenShotFormat==FIF_JPEG)? 420+ (Disp.ScreenShotFormatOpts>>
            (8+(Disp.ScreenShotFormatOpts==0x800)))-(Disp.ScreenShotFormatOpts==0x400)
            : 420+Disp.ScreenShotFormatOpts),
            MF_BYCOMMAND);
#else
          CheckMenuRadioItem(Pop,420,420+format_sl.NumStrings,420+Disp.ScreenShotFormatOpts,MF_BYCOMMAND);
#endif
        }
#endif
        RECT rc;
        GetWindowRect(hBut,&rc);

        SendMessage(hBut,BM_SETCHECK,1,0);
        TrackPopupMenu(Pop,TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
          rc.left,rc.bottom,0,StemWin,NULL);
        if (PasteText.Empty()) SendMessage(hBut,BM_SETCHECK,0,0);
        
        DestroyMenu(Pop);
      }else{
#if defined(SSE_VID_SAVE_NEO)
          if(Disp.ScreenShotFormat==IF_NEO)
          {
            //ASSERT( sizeof(neochrome_file)==32128 );//constant
            VERIFY( (Disp.pNeoFile=new neochrome_file)!=NULL );
            ZeroMemory(Disp.pNeoFile,sizeof(neochrome_file));
            for(int i=0;i<16;i++)
              Disp.pNeoFile->palette[i]=change_endian(STpal[i]);
          }
#endif
        if (runstate==RUNSTATE_RUNNING){
          DoSaveScreenShot|=1;
        }else{
          Disp.SaveScreenShot();
        }
      }
      break;
    }
  }
}
//---------------------------------------------------------------------------
void SetStemWinSize(int w,int h,int xo,int yo)
{
  TRACE_LOG("SetStemWinSize %d %d %d %d\n",xo,yo,w,h);
#if defined(SSE_VID_SDL)
  if(SDL.InUse)
  {
    SDL.LeaveSDLVideoMode();
    SDL.EnterSDLVideoMode();
    //return;
  }
#endif

  if (FullScreen){
    rcPreFS.left=max(rcPreFS.left+xo,0l);
    rcPreFS.top=max(int(rcPreFS.top+yo),-GetSystemMetrics(SM_CYCAPTION));
    rcPreFS.right=rcPreFS.left+w+4+GetSystemMetrics(SM_CXFRAME)*2;
    rcPreFS.bottom=rcPreFS.top+h+MENUHEIGHT+4+GetSystemMetrics(SM_CYFRAME)*2+GetSystemMetrics(SM_CYCAPTION);
  }else{
    if (bAppMaximized==0 && bAppMinimized==0){
      RECT rc;
      GetWindowRect(StemWin,&rc);

      /*
      TRACE_INIT("Window %d %d %d %d\n",
                    max(rc.left+xo,0l),
                    max((int)(rc.top+yo),-GetSystemMetrics(SM_CYCAPTION)),
                    w+4+GetSystemMetrics(SM_CXFRAME)*2,
                    h+MENUHEIGHT+4+GetSystemMetrics(SM_CYFRAME)*2+GetSystemMetrics(SM_CYCAPTION));
*/
      //TRACE_INIT("w+4+GetSystemMetrics(SM_CXFRAME)*2 %d\n",w+4+GetSystemMetrics(SM_CXFRAME)*2);//different in vs2015
      SetWindowPos(StemWin,0,max(rc.left+xo,0l),max((int)(rc.top+yo),-GetSystemMetrics(SM_CYCAPTION)),
                    w+4+GetSystemMetrics(SM_CXFRAME)*2,
                    h+MENUHEIGHT+4+GetSystemMetrics(SM_CYFRAME)*2+GetSystemMetrics(SM_CYCAPTION),
                    SWP_NOZORDER | SWP_NOACTIVATE);
    }else{
      WINDOWPLACEMENT wp;
      wp.length=sizeof(WINDOWPLACEMENT);
      GetWindowPlacement(StemWin,&wp);
      RECT *rc=&wp.rcNormalPosition;
      rc->left=max(int(rc->left+xo),-GetSystemMetrics(SM_CYCAPTION));
      rc->top=max(rc->top+yo,0l);
      rc->right=rc->left+w+4+GetSystemMetrics(SM_CXFRAME)*2;
      rc->bottom=rc->top+h+MENUHEIGHT+4+GetSystemMetrics(SM_CYFRAME)*2+GetSystemMetrics(SM_CYCAPTION);
      SetWindowPlacement(StemWin,&wp);
    }
  }
}
//---------------------------------------------------------------------------
void MoveStemWin(int x,int y,int w,int h)
{
  if (StemWin==NULL) return;

  if (FullScreen){
    if (x==MSW_NOCHANGE) x=rcPreFS.left;
    if (y==MSW_NOCHANGE) y=rcPreFS.top;
    if (w==MSW_NOCHANGE) w=rcPreFS.right-rcPreFS.left;
    if (h==MSW_NOCHANGE) h=rcPreFS.top-rcPreFS.bottom;
    rcPreFS.left=x;rcPreFS.top=y;rcPreFS.right=x+w;rcPreFS.bottom=y+h;
  }else{
    RECT rc;
    GetWindowRect(StemWin,&rc);
    int new_x=rc.left,new_y=rc.top,new_w=rc.right-rc.left,new_h=rc.bottom-rc.top;
    if (x!=MSW_NOCHANGE) new_x=x;
    if (y!=MSW_NOCHANGE) new_y=y;
    if (w!=MSW_NOCHANGE) new_w=w;
    if (h!=MSW_NOCHANGE) new_h=h;
    MoveWindow(StemWin,new_x,new_y,new_w,new_h,true);
  }
}
//---------------------------------------------------------------------------
LRESULT __stdcall FSClipWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar)
{
  switch (Mess){
    case WM_PAINT:
      if (draw_blit()==0){
        RECT dest;
        GetClientRect(Win,&dest);

        PAINTSTRUCT ps;
        BeginPaint(Win,&ps);
        FillRect(ps.hdc,&dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
        EndPaint(Win,&ps);
      }else{
        ValidateRect(Win,NULL);
      }
      return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}
//---------------------------------------------------------------------------
/*  SS Argh! I didn't know Steem did all that just for that fullscreen quit 
    button.
    We place it back on the menu bar because on the lower left, it could trash
    dialog boxes and it's counterintuitive.
    We probably could simplify.
*/
LRESULT __stdcall FSQuitWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar)
{
  bool CheckDown=0;
  switch (Mess){
    case WM_CREATE:
      SetProp(Win,"Down",(HANDLE)0);
      break;
    case WM_DESTROY:
      RemoveProp(Win,"Down");
      break;
    case WM_PAINT:
    {
      RECT rc;
      GetClientRect(Win,&rc);

      PAINTSTRUCT ps;
      BeginPaint(Win,&ps);
      FillRect(ps.hdc,&rc,(HBRUSH)GetSysColorBrush(COLOR_BTNFACE));
#if defined(SSE_VS2008_WARNING_390)
      int Down=int((GetProp(Win,"Down")) ? 1:0);
#else
      int Down=int(bool(GetProp(Win,"Down")) ? 1:0);
#endif
#if defined(SSE_VID_FS_PROPER_QUIT_BUTTON) // we changed the icon
      DrawIconEx(ps.hdc,Down,3+Down,(HICON)hGUIIcon[RC_ICO_FULLQUIT],16,16,0,NULL,DI_NORMAL);
#else
      DrawIconEx(ps.hdc,4+Down,3+Down,(HICON)hGUIIcon[RC_ICO_FULLQUIT],16,16,0,NULL,DI_NORMAL);
      DrawEdge(ps.hdc,&rc,int(Down ? EDGE_SUNKEN:EDGE_RAISED),BF_RECT);
#endif
      EndPaint(Win,&ps);
      return 0;
    }
    case WM_MOUSEMOVE:case WM_CAPTURECHANGED:
      CheckDown=true;
      break;
    case WM_LBUTTONDOWN:
      SetCapture(Win);
      CheckDown=true;
      break;
    case WM_LBUTTONUP:
      ReleaseCapture();
      CheckDown=true;
      PostMessage(Win,WM_USER,0xface,lPar);
      break;
    case WM_USER:
    {
      if (wPar!=0xface) break;

      RECT dest;
      GetClientRect(Win,&dest);
      if (LOWORD(lPar)<dest.right && HIWORD(lPar)<dest.bottom){
        int Quit=IDYES;
        if (FSQuitAskFirst) Quit=Alert(T("Are you sure?"),T("Quit Steem"),MB_ICONQUESTION | MB_YESNO);
        if (Quit==IDYES) QuitSteem();
      }
      return 0;
    }
  }
  if (CheckDown){
#if defined(SSE_VS2008_WARNING_390)
    bool OldDown=(GetProp(Win,"Down")!=0);
#else
    bool OldDown=(bool)GetProp(Win,"Down");
#endif
    bool NewDown=0;

    if (GetCapture()==Win){
      RECT rc;
      GetClientRect(Win,&rc);

      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(Win,&pt);
      NewDown=(pt.x>=0 && pt.x<rc.right && pt.y>=0 && pt.y<rc.bottom);
    }
    if (OldDown!=NewDown){
      SetProp(Win,"Down",(HANDLE)NewDown);
      InvalidateRect(Win,NULL,0);
    }
    return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}
//---------------------------------------------------------------------------
#endif//win32
//---------------------------------------------------------------------------
HRESULT change_fullscreen_display_mode(bool resizeclippingwindow)
{
  HRESULT Ret;
#if defined(SSE_VS2008_WARNING_392) && defined(SSE_VID_D3D)
#elif defined(SSE_VID_BPP_NO_CHOICE)
  int bpp=32;
  if(!SSEConfig.VideoCard32bit)
    bpp=(SSEConfig.VideoCard16bit)?16:8;
#elif defined(SSE_VID_BPP_CHOICE)
  int bpp=SSEConfig.GetBitsPerPixel();
#elif defined(SSE_VID_DD_FS_32BIT)
/*  Fortunately, we only need to specify bpp, Steem already has the correct
    rendering routines.
*/
  int bpp=32; // new default
  if(display_option_8_bit_fs && SSEConfig.VideoCard8bit)
    bpp=8;
  else if(SSEConfig.VideoCard16bit)
    bpp=16;
#else
  int bpp=int(display_option_8_bit_fs ? 8:16);
#endif
#if defined(SSE_VID_D3D)
  RECT rc={0,MENUHEIGHT,monitor_width,monitor_height};
//  int hz256=0;
#if !defined(SSE_VS2008_WARNING_390)
  int hz_ok=0,hz=0;
#endif
#else
  RECT rc={0,MENUHEIGHT,640,480};
#if defined(SSE_VID_BPP_CHOICE) && !defined(SSE_VID_BPP_NO_CHOICE)
  int hz256=int((display_option_fs_bpp==1) ? 0:1);
#else
  int hz256=int(display_option_8_bit_fs ? 0:1);
#endif  
#if !defined(SSE_VID_D3D)
#if defined(SSE_VID_BORDERS_GUI_392)
  int hz_ok=0,hz=prefer_pc_hz[hz256][1+(border!=0)]
#else
  int hz_ok=0,hz=prefer_pc_hz[hz256][1+(border & 1)]
#endif
#endif
  ;
#if !defined(SSE_VID_DISABLE_AUTOBORDER)
  if (border==3){ // auto border on
    border&=~1;
    overscan=0;
    change_window_size_for_border_change(3,border);
  }
#endif
#if defined(SSE_VID_BORDERS_GUI_392)
  if (border){
#else
  if (border & 1){
#endif
    rc.right=800;rc.bottom=600;
  }

#ifndef NO_CRAZY_MONITOR
  if (extended_monitor){
#ifdef BCC_BUILD
    rc.right=max((int)em_width,640);rc.bottom=max((int)em_height,480);hz=0;
#else
    rc.right=max(em_width,640);rc.bottom=max(em_height,480);hz=0;
#endif
  }
#endif
  if (draw_fs_blit_mode==DFSM_LAPTOP){
    rc.right=monitor_width;rc.bottom=monitor_height;
  }
#endif//#if defined(SSE_VID_D3D)
#if defined(SSE_VID_D3D) && defined(SSE_VS2008_WARNING_390)
  if ((Ret=Disp.SetDisplayMode())!=DD_OK) 
#else
  if ((Ret=Disp.SetDisplayMode(rc.right,rc.bottom,bpp,hz,&hz_ok))!=DD_OK) 
#endif
    return Ret;
#if !defined(SSE_VID_D3D)
#if defined(SSE_VID_BORDERS_GUI_392)
  if (hz) tested_pc_hz[hz256][1+(border!=0)]=MAKEWORD(hz,hz_ok);
#else
  if (hz) tested_pc_hz[hz256][1+(border & 1)]=MAKEWORD(hz,hz_ok);
#endif
#endif
#ifdef WIN32
#if !defined(SSE_VID_D3D_2SCREENS) // done in D3DCreateSurfaces()
  SetWindowPos(StemWin,HWND_TOPMOST,0,0,rc.right,rc.bottom,0);
#endif
  if (resizeclippingwindow){
#if defined(SSE_VID_D3D) 
#elif defined(SSE_VID_DD_NO_FS_CLIPPER)
    SetWindowPos(StemWin,0,0,0,rc.right,rc.bottom,SWP_NOZORDER);
#else
    SetWindowPos(ClipWin,0,0,MENUHEIGHT,rc.right,rc.bottom-MENUHEIGHT,SWP_NOZORDER);
#endif
  }

  if (DiskMan.IsVisible()){
    if (DiskMan.FSMaximized){
      SetWindowPos(DiskMan.Handle,NULL,-GetSystemMetrics(SM_CXFRAME),MENUHEIGHT,
                    rc.right+GetSystemMetrics(SM_CXFRAME)*2,
                    rc.bottom+GetSystemMetrics(SM_CYFRAME)-MENUHEIGHT,
                    SWP_NOZORDER | SWP_NOACTIVATE);
    }else{
      SetWindowPos(DiskMan.Handle,NULL,0,0,
                    min(DiskMan.FSWidth,(int)rc.right),min(DiskMan.FSHeight,(int)rc.bottom-MENUHEIGHT),
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
#if !defined(SSE_VID_D3D)
  OptionBox.UpdateHzDisplay();
#endif
  HDC DC=GetDC(StemWin);
  FillRect(DC,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));
  ReleaseDC(StemWin,DC);

#elif defined(UNIX)
#endif

  draw_grille_black=50; // Redraw black areas for 1 second

  return DD_OK;
}
//---------------------------------------------------------------------------
void change_window_size_for_border_change(int oldborder,int newborder)
{
  if (ResChangeResize==0) return;
#if defined(SSE_VID_BORDERS_GUI_392)

  if ((newborder) && !(oldborder)){
    StemWinResize(-(16*4),-(BORDER_TOP*2));
  }else if (!(newborder) && (oldborder)){
    StemWinResize((16*4),(BORDER_TOP*2));
  }

#else
  if ((newborder & 1) && !(oldborder & 1)){
    StemWinResize(-(16*4),-(BORDER_TOP*2));
  }else if (!(newborder & 1) && (oldborder & 1)){
    StemWinResize((16*4),(BORDER_TOP*2));
  }
#endif
}
//---------------------------------------------------------------------------
Str SnapShotGetLastBackupPath()
{
  if (has_extension(LastSnapShot,".sts")==0) return ""; // Just in case folder

  Str Backup=WriteDir+SLASH+GetFileNameFromPath(LastSnapShot);
  char *ext=strrchr(Backup,'.');
  *ext=0;
  Backup+=".stsbackup";
  return Backup;
}
//---------------------------------------------------------------------------
void SnapShotGetOptions(EasyStringList *p_sl)
{
  p_sl->Sort=eslNoSort;
  p_sl->Add(T("&Load Memory Snapshot"),200,0);

  EasyStr NoSaveExplain="";
#ifndef DISABLE_STEMDOS
#if defined(SSE_CPU_LINE_F)
  if (on_rte!=ON_RTE_RTE && on_rte!=ON_RTE_LINE_F){
#else
  if (on_rte!=ON_RTE_RTE){
#endif
    NoSaveExplain=T("The ST is in the middle of a disk operation");
  }else{
    if (stemdos_any_files_open()) NoSaveExplain=T("The ST has file(s) open");
  }
#endif
#if USE_PASTI
  if (NoSaveExplain.Empty()){
    if (hPasti && pasti_active){
      NoSaveExplain=T("The ST is in the middle of a disk operation");
      pastiSTATEINFO psi;
      psi.bufSize=0;
      psi.buffer=NULL;
      psi.cycles=ABSOLUTE_CPU_TIME;
      pasti->SaveState(&psi);
      if (psi.bufSize>0) NoSaveExplain="";
    }
  }
#endif
  if (NoSaveExplain.IsEmpty()){
    p_sl->Add(T("&Save Memory Snapshot"),201,0);
    Str Name=GetFileNameFromPath(LastSnapShot);
    char *ext=strrchr(Name,'.');
    if (ext){
      *ext=0;
      p_sl->Add("-",0,0);
      p_sl->Add(T("Save Over")+" "+Name,205,0);
      if (Exists(SnapShotGetLastBackupPath())){
        p_sl->Add(T("Undo Save Over")+" "+Name,206,0);
      }
    }
  }else{
    p_sl->Add(T("Can't save snapshot because"),0,1);
    p_sl->Add(NoSaveExplain,0,1);
  }
  if (Exists(WriteDir+SLASH+"auto_reset_backup.sts")){
    p_sl->Add("-",0,0);
    p_sl->Add(T("Undo Last Reset"),207,0);
  }
  if (Exists(WriteDir+SLASH+"auto_loadsnapshot_backup.sts")){
    p_sl->Add("-",0,0);
    p_sl->Add(T("Undo Last Memory Snapshot Load"),208,0);
  }

  // Add history
  bool AddedLine=0;
  for (int n=0;n<10;n++){
    if (StateHist[n].NotEmpty()){
      bool FileExists;
#ifdef WIN32
      FileExists=true;
      UINT HostDriveType=GetDriveType(StateHist[n].Lefts(2)+"\\");
      if (HostDriveType==DRIVE_NO_ROOT_DIR){
        FileExists=0;
      }else if (HostDriveType!=DRIVE_REMOVABLE && HostDriveType!=DRIVE_CDROM){
        FileExists=Exists(StateHist[n]);
      }
#else
      FileExists=Exists(StateHist[n]);
#endif
      if (FileExists){
        EasyStr Name=GetFileNameFromPath(StateHist[n]);
        char *dot=strrchr(Name,'.');
        if (dot) *dot=0;

        if (AddedLine==0){
          p_sl->Add("-",0,0);
          AddedLine=true;
        }
        p_sl->Add(Name,210+n,0);
      }
    }
  }
}
#undef LOGSECTION//SS
//---------------------------------------------------------------------------
#ifdef UNIX
#include "x/x_stemwin.cpp"
#endif

