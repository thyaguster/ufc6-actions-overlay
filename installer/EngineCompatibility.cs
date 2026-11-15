// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.IO;
using System.Text;

namespace UFC6OverlaySetup {
    /* The setup recognizes changed PORTHEON file hashes only when the same
       guarded controller callbacks are still present. The DLL repeats the
       checks against the loaded image before installing any hooks. */
    internal static class EngineCompatibility {
        static readonly byte[] NewButton = {0x55,0x41,0x57,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xec,0xc8,0,0,0};
        static readonly byte[] NewAxis = {0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xc8,0,0,0};
        static readonly byte[] NewDisconnect = {0x55,0x41,0x56,0x56,0x57,0x53,0x48,0x83,0xec,0x20,0x48,0x8d,0x6c,0x24,0x20};
        static readonly byte[] OriginalButton = {0x55,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
        static readonly byte[] OriginalAxis = {0x55,0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
        static readonly byte[] OriginalDisconnect = {0x55,0x56,0x57,0x48,0x83,0xec,0x30,0x48,0x8d,0x6c,0x24,0x30,0x48,0xc7,0x45,0xf8,0xfe,0xff,0xff,0xff};
        static readonly byte[] ParsecButton = {0xe9,0x56,0x40,0x99,0x02};
        static readonly byte[] ParsecAxis = {0xe9,0x17,0x3e,0x99,0x02};
        static readonly byte[] ParsecDisconnect = {0xe9,0xb0,0x41,0x99,0x02};
        static bool Equal(byte[] data,int offset,byte[] pattern) {
            if(offset<0||offset>data.Length-pattern.Length)return false;
            for(int i=0;i<pattern.Length;++i)if(data[offset+i]!=pattern[i])return false;
            return true;
        }
        static int Rva(byte[] data,int sectionTable,int sections,uint address) {
            for(int i=0;i<sections;++i){int offset=sectionTable+i*40;uint virtualSize=BitConverter.ToUInt32(data,offset+8),virtualAddress=BitConverter.ToUInt32(data,offset+12),rawSize=BitConverter.ToUInt32(data,offset+16),rawOffset=BitConverter.ToUInt32(data,offset+20);
                if(address<virtualAddress||address-virtualAddress>=virtualSize||address-virtualAddress>=rawSize)continue;
                ulong fileOffset=(ulong)rawOffset+address-virtualAddress;
                return fileOffset<(ulong)data.Length?(int)fileOffset:-1;
            }
            return -1;
        }
        static bool AtRva(byte[] data,int table,int sections,uint rva,byte[] pattern) {return Equal(data,Rva(data,table,sections,rva),pattern);}
        public static bool Matches(string path,string engineFile) {
            try {
                long length=new FileInfo(path).Length;
                if(length<1024||length>64L*1024*1024)return false;
                byte[] data=File.ReadAllBytes(path);
                if(data[0]!='M'||data[1]!='Z')return false;
                int pe=BitConverter.ToInt32(data,0x3c);
                if(pe<0x40||pe>0x1000||pe+0x108>data.Length||BitConverter.ToUInt32(data,pe)!=0x4550)return false;
                int sections=BitConverter.ToUInt16(data,pe+6),optionalSize=BitConverter.ToUInt16(data,pe+20);
                if(BitConverter.ToUInt16(data,pe+4)!=0x8664||BitConverter.ToUInt16(data,pe+24)!=0x20b||sections<1||sections>32)return false;
                int table=pe+24+optionalSize;
                if(table<0||table+sections*40>data.Length)return false;
                uint timestamp=BitConverter.ToUInt32(data,pe+8),image=BitConverter.ToUInt32(data,pe+24+56);
                if(engineFile.Equals("UFC 6.exe",StringComparison.OrdinalIgnoreCase))return image==0x3c03000&&timestamp==0x6adf7fd0&&
                    AtRva(data,table,sections,0x126f040,OriginalButton)&&AtRva(data,table,sections,0x126f270,OriginalAxis)&&AtRva(data,table,sections,0x126ef60,OriginalDisconnect);
                if(engineFile.Equals("UFC 6 Parsec.exe",StringComparison.OrdinalIgnoreCase))return image==0x3c05000&&timestamp==0x6adf7fd0&&
                    AtRva(data,table,sections,0x126f040,ParsecButton)&&AtRva(data,table,sections,0x126f270,ParsecAxis)&&AtRva(data,table,sections,0x126ef60,ParsecDisconnect);
                if(!engineFile.Equals("UFC 6 novo.exe",StringComparison.OrdinalIgnoreCase)||image<0x100000||image>0x80000000)return false;
                int matches=0;
                for(int i=0;i<sections;++i){int offset=table+i*40;uint size=BitConverter.ToUInt32(data,offset+8),rawSize=BitConverter.ToUInt32(data,offset+16),rawOffset=BitConverter.ToUInt32(data,offset+20),flags=BitConverter.ToUInt32(data,offset+36);
                    if((flags&0x20000000)==0||size<0x400||size>0x4000000||rawSize<size||rawOffset>data.Length||size>data.Length-rawOffset)continue;
                    int start=(int)rawOffset,end=start+(int)size;
                    for(int at=start+0x380;at+0x190+NewAxis.Length<=end;++at){
                        if(data[at]!=NewButton[0]||!Equal(data,at,NewButton))continue;
                        if(Equal(data,at+0x190,NewAxis)&&Equal(data,at-0x380,NewDisconnect)&&++matches>1)return false;
                    }
                }
                return matches==1;
            } catch(IOException) {return false;} catch(UnauthorizedAccessException) {return false;} catch(ArgumentException) {return false;} catch(OverflowException) {return false;}
        }
    }
}
