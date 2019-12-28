#include "DriverEntry.h"
#include "ProcessFunc.h"
#include "commonDef.h"
#include <wdm.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <minwindef.h>
#include <ntimage.h>

int i = 0;//进程
ULONG ithread = 0;

int idtnum = 0;//idt
int gdtnum = 0;//gdt
DWORD driverNum = 0;
DWORD ssdtnum = 0;
LDR_DATA_TABLE_ENTRY* pLdrEntry;//遍历模块
struct _PEB* peb;//遍历模块
ULONG modulePID = 0;//遍历哪个进程的模块

PEPROCESS threadPeprocess = NULL;//遍历线程
PDRIVER_OBJECT g_DriverObg;//驱动对象
PLDR_DATA_TABLE_ENTRY pLdr;//遍历进程
LIST_ENTRY* pTemp;//遍历进程
int isSysHook = 0;//是否sysenter hook
ULONG_PTR	g_oldKiFastCallEntery;
ULONG g_uPid = 0;//被保护进程的PID
// 声明函数原型

int isReload = 0;

////////////////////////////////////////////
/////////////  内核重载相关函数   /////////////
////////////////////////////////////////////

static char*		g_pNewNtKernel;		// 新内核
static ULONG		g_ntKernelSize;		// 内核的映像大小
static SSDTEntry*	g_pNewSSDTEntry;	// 新ssdt的入口地址	
static ULONG		g_hookAddr;			// 被hook位置的首地址
static ULONG		g_hookAddr_next_ins;// 被hook的指令的下一条指令的首地址.
void _declspec(naked) disablePageWriteProtect()
{
	_asm
	{
		push eax;
		mov eax, cr0;
		and eax, ~0x10000;
		mov cr0, eax;
		pop eax;
		ret;
	}
}

// 开启内存页写入保护
void _declspec(naked) enablePageWriteProtect()
{
	_asm
	{
		push eax;
		mov eax, cr0;
		or eax, 0x10000;
		mov cr0, eax;
		pop eax;
		ret;
	}
}


// 加载NT内核模块
// 将读取到的缓冲区的内容保存到pBuff中.
NTSTATUS loadNtKernelModule(UNICODE_STRING* ntkernelPath, char** pOut)
{
	NTSTATUS status = STATUS_SUCCESS;
	// 2. 获取文件中的内核模块
	// 2.1 将内核模块作为文件来打开.
	HANDLE hFile = NULL;
	char* pBuff = NULL;
	ULONG read = 0;
	char pKernelBuff[0x1000];

	status = createFile(ntkernelPath->Buffer,
		GENERIC_READ,
		FILE_SHARE_READ,
		FILE_OPEN_IF,
		FALSE,
		&hFile);
	if (status != STATUS_SUCCESS)
	{
		KdPrint(("打开文件失败\n"));
		goto _DONE;
	}

	// 2.2 将PE文件头部读取到内存
	status = readFile(hFile, 0, 0, 0x1000, pKernelBuff, &read);
	if (STATUS_SUCCESS != status)
	{
		KdPrint(("读取文件内容失败\n"));
		goto _DONE;
	}

	// 3. 加载PE文件到内存.
	// 3.1 得到扩展头,获取映像大小. 
	IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)pKernelBuff;
	IMAGE_NT_HEADERS* pnt = (IMAGE_NT_HEADERS*)((ULONG)pDos + pDos->e_lfanew);
	ULONG imgSize = pnt->OptionalHeader.SizeOfImage;

	// 3.2 申请内存以保存各个区段的内容.
	pBuff = ExAllocatePool(NonPagedPool, imgSize);
	if (pBuff == NULL)
	{
		KdPrint(("内存申请失败失败\n"));
		status = STATUS_BUFFER_ALL_ZEROS;//随便返回个错误码
		goto _DONE;
	}

	// 3.2.1 拷贝头部到堆空间
	RtlCopyMemory(pBuff,
		pKernelBuff,
		pnt->OptionalHeader.SizeOfHeaders);

	// 3.3 得到区段头, 并将按照区段头将区段读取到内存中.
	IMAGE_SECTION_HEADER* pScnHdr = IMAGE_FIRST_SECTION(pnt);
	ULONG scnCount = pnt->FileHeader.NumberOfSections;
	for (ULONG i = 0; i < scnCount; ++i)
	{
		//
		// 3.3.1 读取文件内容到堆空间指定位置.
		//
		status = readFile(hFile,
			pScnHdr[i].PointerToRawData,
			0,
			pScnHdr[i].SizeOfRawData,
			pScnHdr[i].VirtualAddress + pBuff,
			&read);
		if (status != STATUS_SUCCESS)
			goto _DONE;

	}


_DONE:
	ZwClose(hFile);
	//
	// 保存新内核的加载的首地址
	//
	*pOut = pBuff;

	if (status != STATUS_SUCCESS)
	{
		if (pBuff != NULL)
		{
			ExFreePool(pBuff);
			*pOut = pBuff = NULL;
		}
	}
	return status;
}


// 修复重定位.
void fixRelocation(char* pDosHdr, ULONG curNtKernelBase)
{
	IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)pDosHdr;
	IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)((ULONG)pDos + pDos->e_lfanew);
	ULONG uRelRva = pNt->OptionalHeader.DataDirectory[5].VirtualAddress;
	IMAGE_BASE_RELOCATION* pRel =
		(IMAGE_BASE_RELOCATION*)(uRelRva + (ULONG)pDos);

	while (pRel->SizeOfBlock)
	{
		typedef struct
		{
			USHORT offset : 12;
			USHORT type : 4;
		}TypeOffset;

		ULONG count = (pRel->SizeOfBlock - 8) / 2;
		TypeOffset* pTypeOffset = (TypeOffset*)(pRel + 1);
		for (ULONG i = 0; i < count; ++i)
		{
			if (pTypeOffset[i].type != 3)
			{
				continue;
			}

			ULONG* pFixAddr = (ULONG*)(pTypeOffset[i].offset + pRel->VirtualAddress + (ULONG)pDos);
			//
			// 减去默认加载基址
			//
			*pFixAddr -= pNt->OptionalHeader.ImageBase;

			//
			// 加上新的加载基址(使用的是当前内核的加载基址,这样做
			// 是为了让新内核使用当前内核的数据(全局变量,未初始化变量等数据).)
			//
			*pFixAddr += (ULONG)curNtKernelBase;
		}

		pRel = (IMAGE_BASE_RELOCATION*)((ULONG)pRel + pRel->SizeOfBlock);
	}
}

// 填充SSDT表
// char* pNewBase - 新加载的内核堆空间首地址
// char* pCurKernelBase - 当前正在使用的内核的加载基址
void initSSDT(char* pNewBase, char* pCurKernelBase)
{
	// 1. 分别获取当前内核,新加载的内核的`KeServiceDescriptorTable`
	//    的地址
	SSDTEntry* pCurSSDTEnt = KeServiceDescriptorTable;
	g_pNewSSDTEntry = (SSDTEntry*)
		((ULONG)pCurSSDTEnt - (ULONG)pCurKernelBase + (ULONG)pNewBase);

	// 2. 获取新加载的内核以下三张表的地址:
	// 2.1 服务函数表基址
	g_pNewSSDTEntry->ServiceTableBase = (ULONG*)
		((ULONG)pCurSSDTEnt->ServiceTableBase - (ULONG)pCurKernelBase + (ULONG)pNewBase);

	// 2.3 服务函数参数字节数表基址
	g_pNewSSDTEntry->ParamTableBase = (ULONG*)
		((ULONG)pCurSSDTEnt->ParamTableBase - (ULONG)pCurKernelBase + (ULONG)pNewBase);

	// 2.2 服务函数调用次数表基址(有时候这个表并不存在.)
	if (pCurSSDTEnt->ServiceCounterTableBase)
	{
		g_pNewSSDTEntry->ServiceCounterTableBase = (ULONG*)
			((ULONG)pCurSSDTEnt->ServiceCounterTableBase - (ULONG)pCurKernelBase + (ULONG)pNewBase);
	}

	// 2.3 设置新SSDT表的服务个数
	g_pNewSSDTEntry->NumberOfService = pCurSSDTEnt->NumberOfService;

	//3. 将服务函数的地址填充到新SSDT表(重定位时其实已经修复好了,
	//   但是,在修复重定位的时候,是使用当前内核的加载基址的, 修复重定位
	//   为之后, 新内核的SSDT表保存的服务函数的地址都是当前内核的地址,
	//   在这里要将这些服务函数的地址改回新内核中的函数地址.)
	disablePageWriteProtect();
	for (ULONG i = 0; i < g_pNewSSDTEntry->NumberOfService; ++i)
	{
		// 减去当前内核的加载基址
		g_pNewSSDTEntry->ServiceTableBase[i] -= (ULONG)pCurKernelBase;
		// 换上新内核的加载基址.
		g_pNewSSDTEntry->ServiceTableBase[i] += (ULONG)pNewBase;
	}
	enablePageWriteProtect();
}
void uninstallHook()
{
	if (g_hookAddr)
	{

		// 将原始数据写回.
		UCHAR srcCode[5] = { 0x2b,0xe1,0xc1,0xe9,0x02 };
		disablePageWriteProtect();

		// 3.1 计算跳转偏移
		// 跳转偏移 = 目标地址 - 当前地址 - 5

		_asm sti
		// 3.2 将跳转指令写入
		RtlCopyMemory(g_hookAddr,
			srcCode,
			5);
		_asm cli
		g_hookAddr = 0;
		enablePageWriteProtect();
	}

	if (g_pNewNtKernel)
	{
		ExFreePool(g_pNewNtKernel);
		g_pNewNtKernel = NULL;
	}
}
ULONG SSDTFilter(ULONG index,/*索引号,也是调用号*/
	ULONG tableAddress,/*表的地址,可能是SSDT表的地址,也可能是Shadow SSDT表的地址*/
	ULONG funAddr/*从表中取出的函数地址*/)
{
	// 如果是SSDT表的话
	if (tableAddress == KeServiceDescriptorTable->ServiceTableBase)
	{
		// 判断调用号(190是ZwOpenProcess函数的调用号)
		if (index == 190)
		{
			// 返回新SSDT表的函数地址
			// 也就是新内核的函数地址.
			return g_pNewSSDTEntry->ServiceTableBase[190];
		}
	}
	// 返回旧的函数地址
	return funAddr;
}
void _declspec(naked) myKiFastEntryHook()
{

	_asm
	{
		pushad; // 压栈寄存器: eax,ecx,edx,ebx, esp,ebp ,esi, edi
		pushfd; // 压栈标志寄存器

		push edx; // 从表中取出的函数地址
		push edi; // 表的地址
		push eax; // 调用号
		call SSDTFilter; // 调用过滤函数

		;// 函数调用完毕之后栈控件布局,指令pushad将
		;// 32位的通用寄存器保存在栈中,栈空间布局为:
		;// [esp + 00] <=> eflag
		;// [esp + 04] <=> edi
		;// [esp + 08] <=> esi
		;// [esp + 0C] <=> ebp
		;// [esp + 10] <=> esp
		;// [esp + 14] <=> ebx
		;// [esp + 18] <=> edx <<-- 使用函数返回值来修改这个位置
		;// [esp + 1C] <=> ecx
		;// [esp + 20] <=> eax
		mov dword ptr ds : [esp + 0x18], eax;
		popfd; // popfd时,实际上edx的值就回被修改
		popad;

		; //执行被hook覆盖的两条指令
		sub esp, ecx;
		shr ecx, 2;
		jmp g_hookAddr_next_ins;
	}
}
void installHook()
{
	g_hookAddr = 0;

	// 1. 找到KiFastCallEntry函数首地址
	ULONG uKiFastCallEntry = 0;
	_asm
	{
		;// KiFastCallEntry函数地址保存
		;// 在特殊模组寄存器的0x176号寄存器中
		push ecx;
		push eax;
		push edx;
		mov ecx, 0x176; // 设置编号
		rdmsr; ;// 读取到edx:eax
		mov uKiFastCallEntry, eax;// 保存到变量
		pop edx;
		pop eax;
		pop ecx;
	}

	// 2. 找到HOOK的位置, 并保存5字节的数据
	// 2.1 HOOK的位置选定为(此处正好5字节,):
	//  2be1            sub     esp, ecx ;
	// 	c1e902          shr     ecx, 2   ;
	UCHAR hookCode[5] = { 0x2b,0xe1,0xc1,0xe9,0x02 }; //保存inline hook覆盖的5字节
	ULONG i = 0;
	for (; i < 0x1FF; ++i)
	{
		if (RtlCompareMemory((UCHAR*)uKiFastCallEntry + i,
			hookCode,
			5) == 5)
		{
			break;
		}
	}
	if (i >= 0x1FF)
	{
		KdPrint(("在KiFastCallEntry函数中没有找到HOOK位置,可能KiFastCallEntry已经被HOOK过了\n"));
		uninstallHook();
		return;
	}


	g_hookAddr = uKiFastCallEntry + i;
	g_hookAddr_next_ins = g_hookAddr + 5;

	// 3. 开始inline hook
	UCHAR jmpCode[5] = { 0xe9 };// jmp xxxx
	disablePageWriteProtect();

	// 3.1 计算跳转偏移
	// 跳转偏移 = 目标地址 - 当前地址 - 5
	*(ULONG*)(jmpCode + 1) = (ULONG)myKiFastEntryHook - g_hookAddr - 5;

	// 3.2 将跳转指令写入
	RtlCopyMemory(uKiFastCallEntry + i,
		jmpCode,
		5);

	enablePageWriteProtect();
}


void ReloadKernel()
{
	isReload = 1;
	// 1. 找到内核文件路径
// 1.1 通过遍历内核链表的方式来找到内核主模块
	LDR_DATA_TABLE_ENTRY* pLdr = ((LDR_DATA_TABLE_ENTRY*)g_DriverObg->DriverSection);
	// 1.2 内核主模块在链表中的第2项.
	for (int i = 0; i < 2; ++i)
		pLdr = (LDR_DATA_TABLE_ENTRY*)pLdr->InLoadOrderLinks.Flink;

	g_ntKernelSize = pLdr->SizeOfImage;

	// 1.3 保存当前加载基址
	char* pCurKernelBase = (char*)pLdr->DllBase;

	KdPrint(("base=%p name=%wZ\n", pCurKernelBase, &pLdr->FullDllName));

	// 2. 读取nt模块的文件内容到堆空间.
	loadNtKernelModule(&pLdr->FullDllName, &g_pNewNtKernel);

	// 3. 修复新nt模块的重定位.
	fixRelocation(g_pNewNtKernel, (ULONG)pCurKernelBase);

	// 4. 使用当前正在使用的内核的数据来填充
	//    新内核的SSDT表.
	initSSDT(g_pNewNtKernel, pCurKernelBase);

	// 5. HOOK KiFastCallEntry,使调用号走新内核的路线
	installHook();

}
//遍历线程
PETHREAD LookupThread(HANDLE hTid)
{
	PETHREAD pEThread = NULL;
	if (STATUS_SUCCESS == PsLookupThreadByThreadId((HANDLE)hTid, &pEThread))
	{
		return pEThread;
	}
	return NULL;
}
////////////////////////////////////////////
/////////////  sysenter hook相关函数   /////////////
////////////////////////////////////////////
// 卸载HOOK
void uninstallSysenterHook()
{
	_asm
	{
		push edx;
		push eax;
		push ecx;
		;// 将新的函数设置进去.
		mov eax, [g_oldKiFastCallEntery];
		xor edx, edx;
		mov ecx, 0x176;
		wrmsr; // 指令使用ecx寄存器的值作为MSR寄存器组的编号,将edx:eax写入到这个编号的寄存器中.
		pop ecx;
		pop eax;
		pop edx;
	}
}

void _declspec(naked) MyKiFastCallEntry()
{
	_asm
	{
		;// 1. 检查调用号
		cmp eax, 0xBE;
		jne _DONE; // 调用号不是0xBE,执行第4步

		;// 2. 检查进程ID是不是要保护的进程的ID
		push eax; // 备份寄存器

		;// 2. 获取参数(进程ID)
		mov eax, [edx + 0x14];// eax保存的是PCLIENT_ID
		mov eax, [eax];// eax保存的是PCLIENT_ID->UniqueProcess

		;// 3. 判断是不是要保护的进程ID
		cmp eax, [g_uPid];
		pop eax;// 恢复寄存器
		jne _DONE;// 不是要保护的进程就跳转

		;// 3.1 是的话就改调用参数,让后续函数调用失败.
		mov dword ptr[edx + 0xC], 0; // 将访问权限设置为0

	_DONE:
		; // 4. 调用原来的KiFastCallEntry
		jmp g_oldKiFastCallEntery;
	}
}
void _declspec(naked) installSysenterHook()
{
	_asm
	{
		push edx;
		push eax;
		push ecx;

		;// 备份原始函数
		mov ecx, 0x176;//SYSENTER_EIP_MSR寄存器的编号.保存着KiFastCallEntry的地址
		rdmsr; // 指令使用ecx寄存器的值作为MSR寄存器组的编号,将这个编号的寄存器中的值读取到edx:eax
		mov[g_oldKiFastCallEntery], eax;// 将地址保存到全局变量中.

		;// 将新的函数设置进去.
		mov eax, MyKiFastCallEntry;
		xor edx, edx;
		wrmsr; // 指令使用ecx寄存器的值作为MSR寄存器组的编号,将edx:eax写入到这个编号的寄存器中.
		pop ecx;
		pop eax;
		pop edx;
		ret;
	}
}


void  Unload(DRIVER_OBJECT* driver) {
	// 删除符号链接
	UNICODE_STRING DosSymName;
	if (isSysHook == 1)
	{
		uninstallSysenterHook();
	}
	if (isReload == 1)
	{
		uninstallHook();

	}
	RtlInitUnicodeString(&DosSymName, L"\\DosDevices\\device_mARK");
	IoDeleteSymbolicLink(&DosSymName);
	// 卸载设备对象
	IoDeleteDevice(driver->DeviceObject);
}

NTSTATUS  Create(DEVICE_OBJECT* device, IRP* irp) {
	device;
	KdPrint(("驱动被创建啦\n"));
	// 设置IRP完成状态
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS  Close(DEVICE_OBJECT* device, IRP* irp) {

	device;
	KdPrint(("驱动被关闭啦\n"));
	// 设置IRP完成状态
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


NTSTATUS  DeviceCtrl(DEVICE_OBJECT* device, IRP* irp)
{
	//_asm int 3;
	device;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG	 complateSize = 0;
	HANDLE		hFile = NULL;

	// 设置IRP完成状态
	irp->IoStatus.Status = STATUS_SUCCESS;

	// 1. 获取本层IO栈
	IO_STACK_LOCATION* ioStack = IoGetCurrentIrpStackLocation(irp);

	// 2. 获取传入的参数:
	// 2.1 IO设备控制码
	ULONG ctrlCode = ioStack->Parameters.DeviceIoControl.IoControlCode;

	// 2.2 输入缓冲区的字节数
	ULONG inputSize = ioStack->Parameters.DeviceIoControl.InputBufferLength;

	// 2.3 获取输出缓冲区的字节数
	ULONG outputSize = ioStack->Parameters.DeviceIoControl.OutputBufferLength;

	// 2.4 获取输入缓冲区
	// 无论在控制码中指定了什么传输方式,都使用irp->AssociatedIrp.SystemBuffer
	// 来保存输入缓冲区的数据
	PVOID pInputBuff = irp->AssociatedIrp.SystemBuffer;

	// 2.5 获取输出缓冲区
	// 如果控制码指定了`METHOD_OUT_DIRECT`, 那么输出缓冲区就以MDL的形式
	// 传入, 如果在调用DeviceIoContral给输出缓冲区传递了NULL时,`MdlAddress`
	// 也会为NULL
	PVOID pOutBuff = NULL;


	if (irp->MdlAddress && METHOD_FROM_CTL_CODE(ctrlCode) & METHOD_OUT_DIRECT) {
		pOutBuff = MmGetSystemAddressForMdlSafe(irp->MdlAddress, 0);
	}


	// 2.6 根据控制码来进行操作
	switch (ctrlCode) // 根据功能号来执行不同功能.
	{
		NTSTATUS			Status = STATUS_SUCCESS;
	case DF_GET_FILE_FIRST: {
			//_asm int 3;
			HANDLE hFind = NULL;
			// 先定义一个缓冲区,用于保存输出的文件内容.
			char tempBuff[sizeof(FILE_BOTH_DIR_INFORMATION) + 267 * 2];
			FILE_BOTH_DIR_INFORMATION* pInfo = (FILE_BOTH_DIR_INFORMATION*)tempBuff;
			// 获取第一个文件
			status = firstFile((wchar_t*)pInputBuff, &hFind, pInfo, sizeof(tempBuff));

			if (status == STATUS_SUCCESS)
			{
				// 将信息输出

				if (pOutBuff != NULL && outputSize >= sizeof(FIND_FILE_INFO))
				{
					FIND_FILE_INFO* pFindInfo = (FIND_FILE_INFO*)pOutBuff;
					pFindInfo->hFindHandle = hFind;

					// 保存信息.
					RtlCopyMemory(pFindInfo->cFileName, pInfo->FileName, pInfo->FileNameLength);
					pFindInfo->cFileName[pInfo->FileNameLength / 2] = 0;
					pFindInfo->dwFileAttributes = pInfo->FileAttributes;
					pFindInfo->createTimeHigh = pInfo->CreationTime.HighPart;
					pFindInfo->createTimeLow = pInfo->CreationTime.LowPart;
					
					pFindInfo->ftLastAccessTime = pInfo->LastAccessTime.QuadPart;
					pFindInfo->writeTimeHigh = pInfo->LastWriteTime.HighPart;
					pFindInfo->writeTimeLow = pInfo->LastWriteTime.LowPart;
					pFindInfo->nFileSizeHigh = pInfo->EndOfFile.HighPart;
					pFindInfo->nFileSizeLow = pInfo->EndOfFile.LowPart;
					/*if (pOutBuff != NULL && outputSize >= sizeof(FIND_FILE_INFO))
					{
						RtlMoveMemory(pOutBuff, pFindInfo, sizeof(FIND_FILE_INFO));
					}*/
					// 设置完成的字节数
					complateSize = sizeof(FIND_FILE_INFO);
				}
			}
		}break;
	case DF_GET_FILE_NEXT: {
			//_asm int 3;
			// 如果输入缓冲区和输出缓冲区不是结构体FIND_FILE_INFO,就退出
			if (inputSize != sizeof(FIND_FILE_INFO) || outputSize != inputSize)
				break;

			// 获取输入缓冲区
			FIND_FILE_INFO* pFindInfo = (FIND_FILE_INFO*)pOutBuff;

			// 先定义一个缓冲区,用于保存输出的文件内容.
			char tempBuff[sizeof(FILE_BOTH_DIR_INFORMATION) + 267 * 2];
			FILE_BOTH_DIR_INFORMATION* pInfo = (FILE_BOTH_DIR_INFORMATION*)tempBuff;
			// 获取第一个文件
			status = nextFile(pFindInfo->hFindHandle, pInfo, sizeof(tempBuff));
			if (status == STATUS_SUCCESS)
			{
				RtlCopyMemory(pFindInfo->cFileName, pInfo->FileName, pInfo->FileNameLength);
				pFindInfo->cFileName[pInfo->FileNameLength / 2] = 0;
				pFindInfo->dwFileAttributes = pInfo->FileAttributes;
				pFindInfo->createTimeHigh = pInfo->CreationTime.HighPart;
				pFindInfo->createTimeLow = pInfo->CreationTime.LowPart;

				pFindInfo->ftLastAccessTime = pInfo->LastAccessTime.QuadPart;
				
				pFindInfo->writeTimeHigh = pInfo->LastWriteTime.HighPart;
				pFindInfo->writeTimeLow = pInfo->LastWriteTime.LowPart;

				pFindInfo->nFileSizeHigh = pInfo->EndOfFile.HighPart;
				pFindInfo->nFileSizeLow = pInfo->EndOfFile.LowPart;
				/*if (pOutBuff != NULL && outputSize >= sizeof(FIND_FILE_INFO))
				{
					RtlMoveMemory(pOutBuff, pFindInfo, sizeof(FIND_FILE_INFO));
				}*/
				// 设置完成的字节数
				complateSize = sizeof(FIND_FILE_INFO);
			}
			else
			{
				ZwClose(pFindInfo->hFindHandle);
			}

		}break;
	case DF_DELETE_FILE:
	{
		//_asm int 3;
		DbgPrint("delete file\n");
		if (pInputBuff)
		{
			__try
			{
				 
				status = removeFile(pInputBuff);
				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_GET_PROCESSNUM:
	{
	
		i = 0;//将进程个数置零
		DbgPrint("Get Process Count\r\n");
		if (pOutBuff)
		{
			__try
			{

				DWORD ProcessCount = 0;
				ProcessCount=(DWORD)APGetProcessNum();
				//DbgPrint("%d\n", ProcessCount);
				RtlMoveMemory(pOutBuff, &ProcessCount, sizeof(DWORD));
				//DbgPrint("%d\n", *(DWORD*)pOutBuff);
				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
		else
		{
			irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}


	}break;
	case DF_ENUM_PROCESS:
	{
		//_asm int 3;
		//DbgPrint("Enum Process\n");
		 if (pOutBuff)
		{
			__try
			{
				PEPROCESS proc = NULL;
				PROCESS_ENTRY_INFO fpi = { 0 };
				
				while ( i <= 0x10000)
				{
					i += sizeof(HANDLE);
					if (STATUS_SUCCESS == PsLookupProcessByProcessId((HANDLE)i, &proc))
					{
						fpi.EProcess = proc;
						fpi.ProcessId = i;
						fpi.ParentProcessId = *(ULONG_PTR*)((ULONG_PTR)proc + 0x140);//WIN64是0x290
						RtlCopyMemory(fpi.szImageName, PsGetProcessImageFileName(proc), 100);
						GetProcessImagePath((ULONG)i, fpi.wzFilePath);//ZwQueryInformationProcess
						if (pOutBuff != NULL && outputSize >= sizeof(PROCESS_ENTRY_INFO))
						{
							RtlMoveMemory(pOutBuff, &fpi, sizeof(PROCESS_ENTRY_INFO));
						}
						// 递减引用计数
						ObDereferenceObject(proc);
						break;
					}
					
					irp->IoStatus.Status = status;
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
		else
		{
			irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}
	
	}break;
	case DF_PROTECT_PRPCESS:
	{
		//_asm int 3;
		//DbgPrint("protect process\n");
		if (pInputBuff)
		{
			__try
			{
				isSysHook = 1;
				RtlMoveMemory(&g_uPid, pInputBuff, sizeof(ULONG));

				installSysenterHook();
				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_TERMINATE_PRPCESS:
	{
		//_asm int 3;
		DbgPrint("terminate process\n");
		if (pInputBuff)
		{
			__try
			{
				ULONG pid = 0;
				//g_uPid = *(ULONG*)pInputBuff;
				RtlMoveMemory(&pid, pInputBuff, sizeof(ULONG));

				HANDLE hProcess = NULL;
				CLIENT_ID ClientId = { 0 };
				OBJECT_ATTRIBUTES objAttribute = { sizeof(OBJECT_ATTRIBUTES) };
				ClientId.UniqueProcess = (HANDLE)pid;
				ClientId.UniqueThread = 0;
				ZwOpenProcess(&hProcess, 1, &objAttribute, &ClientId);
				if (hProcess)
				{
					ZwTerminateProcess(hProcess, 0);
					ZwClose(hProcess);
				}

				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_ENUM_MODULEFIRST:
	{
		//_asm int 3;
		DWORD modulenum = 0;
		
		if (pInputBuff)
		{
			__try
			{
				
				//g_uPid = *(ULONG*)pInputBuff;
				RtlMoveMemory(&modulePID, pInputBuff, sizeof(ULONG));

				PEPROCESS proc = NULL;
				if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)modulePID, &proc)))
				{
					// 遍历模块
					// 1. 找到PEB(由于PEB在用户层空间,因此需要进程挂靠
					KAPC_STATE kapc_status = { 0 };
					KeStackAttachProcess(proc, &kapc_status);
					// 2. 找到PEB.Ldr(模块链表)
					peb = PsGetProcessPeb(proc);
					if (peb != NULL)
					{
						__try {
							// 3. 遍历模块链表
							pLdrEntry = (LDR_DATA_TABLE_ENTRY*)peb->Ldr->InLoadOrderModuleList.Flink;

							LDR_DATA_TABLE_ENTRY* pLdrEntryy = (LDR_DATA_TABLE_ENTRY*)peb->Ldr->InLoadOrderModuleList.Flink;
							LDR_DATA_TABLE_ENTRY* pBegin = pLdrEntryy;
							do
							{
								modulenum++;
								/*KdPrint(("\tBASE:%p SIZE:%06X %wZ\n",
									pLdrEntry->DllBase,
									pLdrEntry->SizeOfImage,
									&pLdrEntry->FullDllName));*/

								// 找到下一个
								pLdrEntryy = (LDR_DATA_TABLE_ENTRY*)pLdrEntryy->InLoadOrderLinks.Flink;
							} while (pBegin != pLdrEntryy);
						}
						__except (EXCEPTION_EXECUTE_HANDLER) {}
					}

					// 解除挂靠
					KeUnstackDetachProcess(&kapc_status);
					// 递减引用计数
					ObDereferenceObject(proc);
				}
				RtlMoveMemory(pOutBuff, &modulenum, sizeof(DWORD));

				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_ENUM_MODULENEXT:
	{
		//__asm int 3;
		DbgPrint("Enum Module\n");
		DbgPrint("terminate process\n");
		if (pOutBuff)
		{
			__try
			{
				PEPROCESS proc = NULL;
				if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)modulePID, &proc)))
				{
					KAPC_STATE kapc_status = { 0 };
					KeStackAttachProcess(proc, &kapc_status);
					if (peb != NULL)
					{
						__try {
							// 3. 遍历模块链表
							//pLdrEntry = (LDR_DATA_TABLE_ENTRY*)peb->Ldr->InLoadOrderModuleList.Flink;
							MODULE_ENTRY temp = { 0 };
							temp.BaseAddress = pLdrEntry->DllBase;
							temp.SizeOfImage = pLdrEntry->SizeOfImage;
							KdPrint(("%wZ\n", &pLdrEntry->FullDllName));
							RtlCopyMemory(temp.wzFilePath, pLdrEntry->FullDllName.Buffer, pLdrEntry->FullDllName.Length);
							temp.wzFilePath[pLdrEntry->FullDllName.Length / 2] = 0;
							RtlCopyMemory(pOutBuff, &temp, sizeof(MODULE_ENTRY));

							pLdrEntry = (LDR_DATA_TABLE_ENTRY*)pLdrEntry->InLoadOrderLinks.Flink;
						}
						__except (EXCEPTION_EXECUTE_HANDLER) {
							DbgPrint("Catch Exception\r\n");
							status = STATUS_UNSUCCESSFUL;
						}
					}
					// 解除挂靠
					KeUnstackDetachProcess(&kapc_status);
					// 递减引用计数
					ObDereferenceObject(proc);
				}

				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_ENUM_THREADFIRST:
	{
		
		DWORD threadnum = 0;
		ithread = 0;
		if (pInputBuff)
		{
			__try
			{

				RtlMoveMemory(&threadPeprocess, pInputBuff, sizeof(ULONG));

				PEPROCESS pEProc = NULL;
				PETHREAD pEThrd = NULL;
				ULONG i = 0;
				for (i = 4; i < 0x25600; i += 4)
				{
					pEThrd = LookupThread((HANDLE)i);
					if (!pEThrd)
					{
						continue;
					}
					pEProc = IoThreadToProcess(pEThrd);
					if (pEProc == threadPeprocess)
					{
						threadnum++;
					}
					ObDereferenceObject(pEThrd);
				}
				RtlMoveMemory(pOutBuff, &threadnum, sizeof(DWORD));

				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_ENUM_THREADNEXT:
	{
		//_asm int 3;
		if (pOutBuff)
		{
			__try
			{

				PEPROCESS pEProc = NULL;
				PETHREAD pEThrd = NULL;
				THREAD_ENTRY temp = { 0 };
				while ( ithread < 0x25600)
				{
					ithread += 4;
					pEThrd = LookupThread((HANDLE)ithread);
					if (!pEThrd)
					{
						continue;
					}
					pEProc = IoThreadToProcess(pEThrd);
					if (pEProc == threadPeprocess)
					{
						temp.Ethread = pEThrd;
						temp.TID = (ULONG)PsGetThreadId(pEThrd);
						break;
					}
					ObDereferenceObject(pEThrd);
				}
				RtlMoveMemory(pOutBuff, &temp, sizeof(THREAD_ENTRY));

				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_GET_DRIVERNUM:
	{
		

		driverNum = 0;
		pLdr = (PLDR_DATA_TABLE_ENTRY)g_DriverObg->DriverSection;
		pTemp = &pLdr->InLoadOrderLinks;
		__try
		{
			PLDR_DATA_TABLE_ENTRY pLdrr = (PLDR_DATA_TABLE_ENTRY)g_DriverObg->DriverSection;
			LIST_ENTRY* pTempp = &pLdrr->InLoadOrderLinks;
			do
			{
				driverNum++;
				pTempp = pTempp->Blink;
			} while (pTempp != &pLdrr->InLoadOrderLinks);
			RtlMoveMemory(pOutBuff, &driverNum, sizeof(DWORD));
			irp->IoStatus.Status = status;
		}
		
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DbgPrint("Catch Exception\r\n");
			status = STATUS_UNSUCCESSFUL;
		}
	}break;
	case DF_ENUM_DRIVER:
	{
		if (pOutBuff)
		{
			__try
			{
				{
					PLDR_DATA_TABLE_ENTRY pDriverInfo = (PLDR_DATA_TABLE_ENTRY)pTemp;
					DRIVER_ENTRY_INFO temp = { 0 };
					temp.BaseAddress = pDriverInfo->DllBase;
				
					temp.Size = pDriverInfo->SizeOfImage;
					RtlStringCchCopyW(temp.wzDriverPath, pDriverInfo->FullDllName.Length / sizeof(WCHAR) + 1,
						(WCHAR*)pDriverInfo->FullDllName.Buffer);
					RtlStringCchCopyW(temp.wzDriverName, pDriverInfo->BaseDllName.Length / sizeof(WCHAR) + 1,
						(WCHAR*)pDriverInfo->BaseDllName.Buffer);
				
					if (pOutBuff != NULL && outputSize >= sizeof(DRIVER_ENTRY_INFO))
					{
						RtlMoveMemory(pOutBuff, &temp, sizeof(DRIVER_ENTRY_INFO));
					}

					pTemp = pTemp->Blink;
				}
				irp->IoStatus.Status = STATUS_SUCCESS;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				Status = STATUS_UNSUCCESSFUL;
			}
		}
		else
		{
			irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}

	}break;
	case DF_HIDE_DRIVER:
	{
	

		if (pInputBuff)
		{
			__try
			{
				UNICODE_STRING driverName;
				RtlInitUnicodeString(&driverName, (PCWSTR)pInputBuff);
				DbgPrint("driver：%wZ\n", driverName.Buffer);

				HideDriver(g_DriverObg, &driverName);
				irp->IoStatus.Status = status;
			}

			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
		
	}break;
	case DF_GET_IDTFIRST:
	{
		idtnum = 0;
	}break;
	case DF_GET_IDT:
	{
		//_asm int 3;
		DbgPrint("Get IDT\n");
		
		if (pOutBuff)
		{
			__try
			{
				idtnum++;
				IDT_INFO SIDT = { 0 };
				PIDT_ENTRY pIDTEntry = NULL;
				ULONG uAddr = 0;
				//获取IDT表的地址
				_asm sidt SIDT;
				//获取IDT表数组地址
				pIDTEntry = (PIDT_ENTRY)MAKELONG(SIDT.uLowIdtBase, SIDT.uHighIdtBase);
				//for (; idtnum < 0x100; idtnum++)
				{
					if (pOutBuff != NULL && outputSize >= sizeof(IDT_ENTRY))
					{
						RtlMoveMemory(pOutBuff, &pIDTEntry[idtnum], sizeof(IDT_ENTRY));
					}
				}
				irp->IoStatus.Status = status;
			}

			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_HIDE_PROCESS:
	{
		//_asm int 3;
		if (pInputBuff)
		{
			__try
			{
				ULONG pid = 0;
				RtlMoveMemory(&pid, pInputBuff, sizeof(ULONG));

				HideProcess(pid);
				irp->IoStatus.Status = status;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_GET_GDTFIRST:
	{
		gdtnum = 0;
	}break; 
	case DF_GET_GDT:
	{
		DbgPrint("Get GDT\n");

		if (pOutBuff)
		{
			__try
			{
				gdtnum++;
				GDT_INFO SGDT = { 0 };
				PGDT_ENTRY pGDTEntry = NULL;
				ULONG uAddr = 0;
				//获取GDT表的地址
				_asm sgdt SGDT;
				//获取GDT表数组地址
				pGDTEntry = (PGDT_ENTRY)MAKELONG(SGDT.uLowGdtBase, SGDT.uHighGdtBase);
				//for (; idtnum < 0x100; idtnum++)
				{
					if (pOutBuff != NULL && outputSize >= sizeof(GDT_ENTRY))
					{
						RtlMoveMemory(pOutBuff, &pGDTEntry[gdtnum], sizeof(GDT_ENTRY));
					}
				}
				irp->IoStatus.Status = status;
			}

			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				status = STATUS_UNSUCCESSFUL;
			}
		}
	}break;
	case DF_GET_SSDT_COUNT:
	{
		ssdtnum = 0;
		__try
		{

			DWORD ssdtn = KeServiceDescriptorTable->NumberOfService;
			RtlMoveMemory(pOutBuff, &ssdtn, sizeof(DWORD));
			irp->IoStatus.Status = status;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DbgPrint("Catch Exception\r\n");
			status = STATUS_UNSUCCESSFUL;
		}
	}break;
	case DF_ENUM_SSDT:
	{
		DbgPrint("Enum SsdtHook\r\n");
		//_asm int 3;
		if (pOutBuff)
		{
			__try
			{
				LONG addr = GetFunticonAddr(KeServiceDescriptorTable,ssdtnum);
				//DbgPrint("address[%d]:0x%08X\n", ssdtnum,KeServiceDescriptorTable->ServiceTableBase + ssdtnum * 4);
				RtlMoveMemory(pOutBuff, &addr, sizeof(LONG));
				
				ssdtnum++;

				irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				DbgPrint("Catch Exception\r\n");
				Status = STATUS_UNSUCCESSFUL;
			}
		}
		else
		{
			irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}
	}break;
	case DF_KERNEL_RELOAD:
	{
		_asm int 3;
		__try
		{
			ReloadKernel();

			irp->IoStatus.Status = STATUS_SUCCESS;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DbgPrint("Catch Exception\r\n");
			Status = STATUS_UNSUCCESSFUL;
		}
	}break;
	}
	irp->IoStatus.Information = complateSize;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS DriverEntry(DRIVER_OBJECT* driver, UNICODE_STRING* path)
{
	path;
	_asm int 3;
	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING NtDevName;
	RtlInitUnicodeString(&NtDevName, L"\\Device\\mydevice");

	// 1. 创建一个设备对象.否则无法绑定符号链接,没有符号链接, 用户层就连接不到内核层
	DEVICE_OBJECT* dev = NULL;
	status = IoCreateDevice(driver,
		0,
		&NtDevName,
		FILE_DEVICE_UNKNOWN,
		0,
		0,
		&dev);

	dev->Flags |= DO_DIRECT_IO; // 使用直接IO

	if (status != STATUS_SUCCESS)
		return status;

	UNICODE_STRING DosSymName;
	RtlInitUnicodeString(&DosSymName, L"\\DosDevices\\device_mARK");
	//2. 为设备对象绑定符号链接
	status = IoCreateSymbolicLink(&DosSymName, &NtDevName);
	if (NT_SUCCESS(status))
	{
		DbgPrint("mARK is Starting!!!\n");
	}
	//绑定卸载函数
	driver->DriverUnload = Unload;
	g_DriverObg = driver;
	/*isReload = 1;
	ReloadKernel();*/


	// 绑定派遣函数, 如果不绑定这两个函数, 用户层在打开驱动时会失败.
	driver->MajorFunction[IRP_MJ_CREATE] = Create;
	driver->MajorFunction[IRP_MJ_CLOSE] = Close;
	driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceCtrl;

	return status;

}
