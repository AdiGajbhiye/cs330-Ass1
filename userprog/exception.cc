// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"
#include "machine.h"
#include "translate.h"
#include "addrspace.h"
#include "thread.h"
#include "system.h"
#include "list.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

List waitingList;
static void func(int dummy){
    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
        threadToBeDestroyed = NULL;
    }
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {         // if there is an address space

        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreStateOnSwitch();
    }
#endif
    machine->Run();
}

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;        // Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SYScall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SYScall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
	  writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
	     writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
	     writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_PrintChar)) {
	writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetReg)) {
       int regno = machine->ReadRegister(4);
       int regval = machine->ReadRegister(regno); 
       machine->WriteRegister(2,regval); 
      
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetPA)) {
       unsigned int virtual_add = machine->ReadRegister(4);
       unsigned int vpn = (unsigned) virtual_add/PageSize;
       unsigned int offset = (unsigned) virtual_add % PageSize;
       TranslationEntry *entry;
       if(vpn >= machine->pageTableSize){
          machine->WriteRegister(2,-1);
       }
       else if(!(machine->NachOSpageTable[vpn]).valid){
          machine->WriteRegister(2,-1);
       }
       else{
	  entry = &(machine->NachOSpageTable[vpn]);
	  unsigned int pageFrame = entry->physicalPage;
	  if(pageFrame >= NumPhysPages)  machine->WriteRegister(2,-1);
	  else{
	     int physAddr = pageFrame * PageSize + offset;  
	     machine->WriteRegister(2,physAddr);
	  }
       }
       
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }   
    else if ((which == SyscallException) && (type == SYScall_GetPID)) {
       machine->WriteRegister(2,currentThread->getPID());	
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetPPID)) {
       machine->WriteRegister(2,currentThread->getPPID());	
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Time)) {
       machine->WriteRegister(2,stats->totalTicks);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }   
    else if ((which == SyscallException) && (type == SYScall_Yield)) {
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       IntStatus oldLevel = interrupt->SetLevel(IntOff);
       currentThread->YieldCPU();
       (void) interrupt->SetLevel(oldLevel);
    }   
    else if ((which == SyscallException) && (type == SYScall_Sleep)) {
       int sleepTicks = machine->ReadRegister(4);
       if(sleepTicks == 0) { currentThread->YieldCPU(); }
       else{
	  int wakeupTime = sleepTicks + stats->totalTicks;
          waitingList.SortedInsert((void *)currentThread,wakeupTime);
          IntStatus oldLevel = interrupt->SetLevel(IntOff);
          currentThread->PutThreadToSleep();
          (void) interrupt->SetLevel(oldLevel);
       }   
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Exec)) {
       char filename[50];
       int fileAddr = machine->ReadRegister(4);
       int i = 0;
       machine->ReadMem(fileAddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  filename[i] = (*(char*)&memval);
          fileAddr++;
	  i++;
          machine->ReadMem(fileAddr, 1, &memval);
       }
       OpenFile *executable = fileSystem->Open(filename);
       ProcessAddrSpace *space;

       space = new ProcessAddrSpace(executable);    
       currentThread->space = space;

       delete executable;			// close file

       space->InitUserCPURegisters();		// set the initial register values
       space->RestoreStateOnSwitch();		// load page table register

       machine->Run();			// jump to the user progam
       ASSERT(FALSE);
    }   
    else if ((which == SyscallException) && (type == SYScall_Fork)) {
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       NachOSThread *newThread;
       newThread = new NachOSThread("child");
       currentThread->InitializeChild(newThread->getPID());
       ProcessAddrSpace *space;
       space = new ProcessAddrSpace((currentThread->space)->getStartPage(), 
				    (currentThread->space)->getVirtualPages());
       newThread->space = space;
       machine->WriteRegister(2,0);
       newThread->SaveUserState();
       machine->WriteRegister(2,newThread->getPID());
       newThread->ThreadFork(func,0);
    }   
    else if ((which == SyscallException) && (type == SYScall_Exit)) {
       int status = machine->ReadRegister(4);
       if (threadCount == 1) interrupt->Halt();
       if (status == PARENT_WAITING || status == CHILD_LIVE)
           status = NORMAL_EXIT;
       
       if (currentThread->parent != NULL) {
           int i, currentStatus;
           for (i = 0;i < (currentThread->parent)->childCount; i++) {
               if ((currentThread->parent)->childPID[i] == currentThread->getPID()) {
                   currentStatus = (currentThread->parent)->childStatus[i];
                   (currentThread->parent)->childStatus[i] = status;
                   break;
               }
            }
            if (currentStatus == PARENT_WAITING) {
                IntStatus oldLevel = interrupt->SetLevel(IntOff);
                scheduler->ThreadIsReadyToRun(currentThread->parent);
                (void) interrupt->SetLevel(oldLevel);
            }
       }
        
       currentThread->FinishThread();       
    }
    else if ((which == SyscallException) && (type == SYScall_Join)) {
       int childPID = machine->ReadRegister(4);
       int i, childFound = 0, status;
       for (i = 0; i < currentThread->childCount; i++) {
           if (currentThread->childPID[i] == childPID) {
               childFound = 1;
               status = currentThread->childStatus[i];
               if (status == CHILD_LIVE){
                   currentThread->childStatus[i] = PARENT_WAITING;
                   IntStatus oldLevel = interrupt->SetLevel(IntOff);
                   currentThread->PutThreadToSleep();
                   (void) interrupt->SetLevel(oldLevel);
               }
               break;
           }
       }
       status = currentThread->childStatus[i];
       while (status == PARENT_WAITING) {
           IntStatus oldLevel = interrupt->SetLevel(IntOff);
           currentThread->PutThreadToSleep();
           (void) interrupt->SetLevel(oldLevel);
           status = currentThread->childStatus[i];
       }    
       if(childFound == 0)  status = -1; 
       machine->WriteRegister(2,status);  
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_NumInstr)) {
       machine->WriteRegister(2,currentThread->numInstr);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}

