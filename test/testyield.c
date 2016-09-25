#include "syscall.h"

int
main()
{
    int x, i=0;

    x = system_call_Fork();
//    while (i<1) {
//	i++;
       system_call_PrintString("*** thread ");
       system_call_PrintInt(system_call_GetPID());
       system_call_PrintString(" looped ");
       system_call_PrintInt(i);
       system_call_PrintString(" instructions ");
       system_call_Yield();
       system_call_PrintString("*** thread ");
       system_call_PrintInt(system_call_GetPID());
       system_call_PrintString(" looped ");
       system_call_PrintInt(i);
       system_call_PrintString("iiiiiiii instructions ");
       system_call_Yield();
       system_call_PrintInt(system_call_GetPID());
       system_call_PrintInt(i);
       system_call_Yield();
  //  }
    if (x != 0) {
       system_call_PrintString("Before join.\n");
       system_call_Join(x);
       system_call_PrintString("After join.\n");
    }
    return 0;
}
