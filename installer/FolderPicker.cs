// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.IO;
using System.Runtime.InteropServices;
namespace UFC6OverlaySetup {
 public static class FolderPicker {
  [ComImport,Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface ShellItem {
   void BindToHandler(IntPtr context,ref Guid handler,ref Guid iid,out IntPtr result);
   void GetParent(out ShellItem parent);
   void GetDisplayName(uint kind,out IntPtr name);
   void GetAttributes(uint mask,out uint attributes);
   void Compare(ShellItem other,uint hint,out int order);
  }
  [ComImport,Guid("D57C7288-D4AD-4768-BE02-9D969532D960"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface OpenDialog {
   [PreserveSig] int Show(IntPtr owner);
   void SetFileTypes(uint count,IntPtr filters);
   void SetFileTypeIndex(uint index);void GetFileTypeIndex(out uint index);
   void Advise(IntPtr events,out uint cookie);void Unadvise(uint cookie);
   void SetOptions(uint options);void GetOptions(out uint options);
   void SetDefaultFolder(ShellItem folder);void SetFolder(ShellItem folder);
   void GetFolder(out ShellItem folder);void GetCurrentSelection(out ShellItem selection);
   void SetFileName([MarshalAs(UnmanagedType.LPWStr)]string name);
   void GetFileName(out IntPtr name);
   void SetTitle([MarshalAs(UnmanagedType.LPWStr)]string title);
   void SetOkButtonLabel([MarshalAs(UnmanagedType.LPWStr)]string text);
   void SetFileNameLabel([MarshalAs(UnmanagedType.LPWStr)]string text);
   void GetResult(out ShellItem result);
  }
  [DllImport("shell32.dll",CharSet=CharSet.Unicode,PreserveSig=true)]
  static extern int SHCreateItemFromParsingName(string name,IntPtr context,ref Guid iid,out ShellItem item);
  public static string Select(IntPtr owner,string initial){
   var dialog=(OpenDialog)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("DC1C5A9C-E88A-4DDE-A5A1-60F82A20AEF7")));
   ShellItem selected=null,start=null;
   try{
    uint flags;dialog.GetOptions(out flags);dialog.SetOptions(flags|0x20u|0x40u|0x800u|0x8u);
   dialog.SetTitle("Select PORTHEON\\games\\UFC6 or its bin folder");dialog.SetOkButtonLabel("Select folder");
    if(Directory.Exists(initial)){
     Guid iid=new Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE");
     if(SHCreateItemFromParsingName(Path.GetFullPath(initial),IntPtr.Zero,ref iid,out start)==0)dialog.SetFolder(start);
    }
    int result=dialog.Show(owner);if(result==unchecked((int)0x800704C7))return null;if(result<0)Marshal.ThrowExceptionForHR(result);
    dialog.GetResult(out selected);IntPtr path;selected.GetDisplayName(0x80058000u,out path);
    try{return Marshal.PtrToStringUni(path);}finally{Marshal.FreeCoTaskMem(path);}
   }finally{if(selected!=null)Marshal.ReleaseComObject(selected);if(start!=null)Marshal.ReleaseComObject(start);Marshal.ReleaseComObject(dialog);}
  }
 }
}
