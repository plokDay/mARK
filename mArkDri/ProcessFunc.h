#pragma once



NTSTATUS removeFile(wchar_t* filepath);
UINT32 APGetProcessNum();
CHAR* PsGetProcessImageFileName(IN PEPROCESS Process);
NTSTATUS GetProcessImagePath(ULONG ulProcessId, WCHAR*  ProcessImagePath);//获得完整路径
NTSTATUS firstFile(wchar_t* dir, HANDLE* hFind, FILE_BOTH_DIR_INFORMATION* fileInfo, int size);
NTSTATUS nextFile(HANDLE hFind, FILE_BOTH_DIR_INFORMATION* fileInfo, int size);
NTSTATUS createFile(wchar_t * filepath,
	ULONG access,
	ULONG share,
	ULONG openModel,
	BOOLEAN isDir,
	HANDLE * hFile);
int HideDriver(PDRIVER_OBJECT pDriverObj, PUNICODE_STRING uniDriverName);
#pragma pcka(1)
typedef struct
{
	ULONG* ServiceTableBase;
	ULONG* ServiceCounterTableBase;
	ULONG NumberOfService;
	ULONG* ParamTableBase;
}SSDTEntry, *PSSDTEntry;
#pragma pcka()
//导出系统服务描述符表SSDT的指针  
extern  PSSDTEntry  KeServiceDescriptorTable;
LONG GetFunticonAddr(PSSDTEntry KeServiceDescriptorTable, LONG lgSsdtIndex);
void HideProcess(ULONG hidePID);
NTSTATUS readFile(HANDLE hFile,
	ULONG offsetLow,
	ULONG offsetHig,
	ULONG sizeToRead,
	PVOID pBuff,
	ULONG* read);