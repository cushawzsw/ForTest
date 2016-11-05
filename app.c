/*
*********************************************************************************************************
*                                              EXAMPLE CODE
*
*                          (c) Copyright 2003-2013; Micrium, Inc.; Weston, FL
*
*               All rights reserved.  Protected by international copyright laws.
*               Knowledge of the source code may NOT be used to develop a similar product.
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
*                                     ST Microelectronics STM32
*                                              on the
*
*                                     Micrium uC-Eval-STM32F107
*                                        Evaluation Board
*
* Filename      : app.c
* Version       : V1.00
* Programmer(s) : EHS
*                 DC
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include <includes.h>

#include "SysCfg.h"
#include "ui.h"
#include "usbh_core.h"
#include "usbh_usr.h"
#include "usbh_msc_core.h"
#include "debugA.h"

#define UPDATE_ENABLE		0		// 1=允许程序升级,0=调试状态暂时关闭程序升级
#define PULSE_GPS_ENABLE	1		// 脉冲模式GPS功能使能标志

#if UPDATE_ENABLE > 0
#define MSG1    "__APP_START_INFO__"
const BOOL g_bUpdateEnable = true;
#else 
#define MSG1     ""
const BOOL g_bUpdateEnable = false;
#endif

#if PULSE_GPS_ENABLE > 0
const BOOL g_bPulseGpsEnable = true;
#else 
const BOOL g_bPulseGpsEnable = false;
#endif

#define IWEG_ENABLE		1

#if IWEG_ENABLE > 0
#define FeedDog()   IWDG_ReloadCounter()
#else
#define FeedDog()
#endif

__attribute__((used)) const struct
{
  char AppStart[sizeof(MSG1)];
  char VenderInfo[sizeof(MSG2)];
  char DeviceName[sizeof(MSG3)];
  char DeviceType[sizeof(MSG4)];
  char BoardType[sizeof(MSG5)];
  char SoftwareInfo[sizeof(MSG6)];
  char HardwareInfo[sizeof(MSG7)];
  char AppEnd[sizeof(MSG8)];
}App_Info_TypeDef =
{
    MSG1,
    MSG2,
    MSG3,
    MSG4,
    MSG5,
    MSG6,
    MSG7,
    MSG8,
};

SYS_PARAS		g_tSysParas;
//tGPS			g_tGps;
tBOARD			g_tBoard;
tNET_PARAS	g_tNet;

USB_OTG_CORE_HANDLE          USB_OTG_Core;
USBH_HOST                    USB_Host;

volatile BOOL  bScreenShots;

extern void EnterWork(void);
extern int IsReBoot(void);
extern void SystemLowPower(void);
extern void SystemNormalPower(void);
extern void SaveWorkParasToW25qx(void);
extern void Dbg_Init(void);
extern void BoardTaskDel(void);
extern void SaveWorkParas(void);
void GetTerminalStateTick(void);
void LCD_PowerOff(void);
void IWDG_Init(void);
void MonitorBattery(u32 interval);
void MonitorDCIN(void);
void MonitorGPS_Per10ms(void);
void ModuleTaskDel(void);
void MonitorBuzzer(void);
void  App_OS_SetAllHooks   (void);
volatile BOOL g_RxBoardError = true;



//const u32 x1[1024] __attribute__((at(0x8020000))) = {~0};

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/
#define     APP_TASK_START_PRIO         2
#define     MONITOR_TASK_PRIO           2
#define     BOARD_TASK_PRIO             3
#define     MODULE_TASK_PRIO            4
#define     UI_TASK_PRIO                7
#define     USB_TASK_PRIO               43

#define     APP_TASK_START_STK_SIZE     256
#define     MONITOR_TASK_STK_SIZE       256
#define     BOARD_TASK_STK_SIZE         256
#define     MODULE_TASK_STK_SIZE        256
#define     UI_TASK_STK_SIZE            1024
#define     USB_TASK_STK_SIZE           4096
/*
*********************************************************************************************************
*                                                 TCB
*********************************************************************************************************
*/

static  OS_TCB   AppTaskStartTCB;
//static  OS_TCB   MonitorTaskTCB;
//static  OS_TCB   W25QXTaskTCB;
static  OS_TCB   USBTaskTCB;
OS_TCB   UITaskTCB;

extern  OS_TCB   BoardTaskTCB;
extern  OS_TCB   ModuleTaskTCB;
volatile u32 g_b4GEnable;

/*
*********************************************************************************************************
*                                                STACKS
*********************************************************************************************************
*/

CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
//CPU_STK  DataTaskStk[DATA_TASK_STK_SIZE]; // __attribute__ ((section ("FAST_RAM")));
//CPU_STK  MonitorTaskStk[MONITOR_TASK_STK_SIZE];
CPU_STK  ModuleTaskStk[MODULE_TASK_STK_SIZE];
CPU_STK  UITaskStk[UI_TASK_STK_SIZE];
CPU_STK  BoardTaskStk[BOARD_TASK_STK_SIZE];
CPU_STK  USBTaskStk[USB_TASK_STK_SIZE];


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskCreate (void);
static  void  AppObjCreate  (void);
static  void  AppTaskStart  (void *p_arg);
//static  void  DataTask  (void *p_arg);
//static  void  MonitorTask  (void *p_arg);
static  void  USBTask  (void *p_arg);

extern  void  ModuleTask  (void *p_arg);
extern  void  UITask  (void *p_arg);
extern  void  BoardTask  (void *p_arg);

extern void MainTask(void);
extern void  TaskNetCreate (void);
extern void TaskNetDelete(void);

static  void  UITaskCreate (void);
//static  void  W25QXTaskCreate (void);
//static  void  MonitorTaskCreate (void);
static  void  ModuleTaskCreate (void);
static  void  BoardTaskCreate (void);
static  BOOL CheckPowerKey(void);
static  void  USBTaskCreate (void);
static void GetW25qxParas(void);
static void MonitorLed(void);
/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*********************************************************************************************************
*/

int  main (void)
{
    OS_ERR  err;
    
    BSP_Init();

//    BSP_IntDisAll();                                            /* Disable all interrupts.                              */
    CPU_IntDis(); 

    OSInit(&err);                                               /* Init uC/OS-III.                                      */

		App_OS_SetAllHooks();
	
    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,                /* Create the start task                                */
                 (CPU_CHAR   *)"App Task Start",
                 (OS_TASK_PTR ) AppTaskStart,
                 (void       *) 0,
                 (OS_PRIO     ) APP_TASK_START_PRIO,
                 (CPU_STK    *)&AppTaskStartStk[0],
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE / 10,
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
                 (OS_ERR     *)&err);

    OSStart(&err);                                              /* Start multitasking (i.e. give control to uC/OS-III). */
}


/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

extern void Uart_Init(void);
extern void TSC_Init(void);

static  void  AppTaskStart (void *p_arg)
{
  OS_ERR      err;
  u32 u32StateCtr10ms = 0;
	u32 u32BootDly = 30;
	u32 u32BattDly;
	int bRebootFlag;

   (void)p_arg;
	
	bRebootFlag = IsReBoot();
	
	if (bRebootFlag) {
		POWER_HOLD(1);
		ExtPowerOn();
	}
    
//    BSP_Init();    
	
    CPU_Init();

//     cpu_clk_freq = BSP_CPU_ClkFreq();                           /* Determine SysTick reference freq.                    */
//     cnts = cpu_clk_freq / (CPU_INT32U)OSCfg_TickRate_Hz;        /* Determine nbr SysTick increments                     */
//     OS_CPU_SysTickInit(cnts);                                   /* Init uC/OS periodic time src (SysTick).              */
    SysTickInit ();
    
    Mem_Init();                                                 /* Initialize Memory Management Module                  */

#if OS_CFG_STAT_TASK_EN > 0u
    OSStatTaskCPUUsageInit(&err);                               /* Compute CPU capacity with no task running            */
#endif

//    CPU_IntDisMeasMaxCurReset();
    
    AppTaskCreate();                                            /* Create Application Tasks                             */
    
    AppObjCreate();                                             /* Create Application Objects                           */
  
  g_tSysParas.bPowerHold = false;
	
	if (ReadPowerHoldPin()) {
		POWER_HOLD(1);	// 更新程序后重启
		u32BootDly = 50;
	} else {
		u32BootDly = 150;
	}

	TSC_Init();
	Uart_Init();
	ADC_Batt_Init();
		
	// 延时300ms启动
	if (bRebootFlag == 0)
	{
		u32 i;
		for (i=0; i<u32BootDly; i++) {
			MonitorBattery(1);
			//MonitorDCIN();
			OSTimeDly(2,OS_OPT_TIME_PERIODIC,&err);
		}
	}
	else
	{
		u32 i;
		for (i=0; i<10; i++) {
			MonitorBattery(1);
		}
	}
  
  /***************************获取开机键状态***************************************/
  if (Read_PWK()) {
    g_tSysParas.bPwkUp = false; // 开机键未弹起
  } else {
    g_tSysParas.bPwkUp = true;  // 开机键已弹起
  }
	
	GetW25qxParas();  // 读w25qx参数
	g_b4GEnable = g_tNet.b4GEnable;
	
	UITaskCreate();
	
#if IWEG_ENABLE > 0
	IWDG_Init();
#endif
	
	g_tSysParas.eSysState = SYS_STATE_VERIFY;
	
	if (bRebootFlag)
	{
		u32StateCtr10ms = 0;
		ExtPowerOn();
		ModuleTaskCreate();
		BoardTaskCreate();
		if (g_b4GEnable) {
			TaskNetCreate();
		}
		Reboot_EnterWork();
		g_tSysParas.eSysState = SYS_STATE_WORK;
	}
	
	u32BattDly = 300;
	
  while (DEF_TRUE)
  {
		FeedDog();	// 喂狗
		
		if (u32BattDly)
		{
			u32BattDly -= 1;
			MonitorBattery(1);
		}
		else
		{
			MonitorBattery(500);	// 每隔100个周期(1s)检测一次电池电压
		}
		MonitorDCIN();
		MonitorBuzzer();
		MonitorLed();
		
		tWorkParas.CurRunTime += 1;
		if ((tWorkParas.CurRunTime %  180000) == 0) {
			SaveWorkParas();
		}
		
// 		if ((tWorkParas.CurRunTime %  100) == 0) {
// 			g_u32BattCtr += 1;
// 		}
		
    /***************************开机确认状态*******************************************/
    if (g_tSysParas.eSysState == SYS_STATE_VERIFY)
    {
			POWER_HOLD(1);
      g_tSysParas.u32RunCtr = 0;
      g_tSysParas.bPowerOff = false;
      g_tSysParas.bPowerHold = true;
			
			if ((g_tSysParas.u32Battery < 16) && (g_bUpdateEnable == true))
			{
				u32StateCtr10ms = 0;
				g_tSysParas.eSysState = SYS_STATE_STANDBY;  // 开机欠压进入待机状态并在界面上显示"电池欠压"
				SetUIPageId(PAGE_WIN, DISP_ID_BATTOFF);
			}
			else
			{
				SetUIPageId(PAGE_WIN, DISP_ID_LOGO);
				g_tSysParas.eSysState = SYS_STATE_WORK;
				u32StateCtr10ms = 0;
				ExtPowerOn();
				ModuleTaskCreate();
				BoardTaskCreate();
				if (g_b4GEnable) {
					TaskNetCreate();
				}
				
				ModuleVoice(VOICE_SN_BOOTUP); // 开机提示音
			}
    }

    /***************************工作状态***********************************************/
    else if (g_tSysParas.eSysState == SYS_STATE_WORK)
    {
			POWER_HOLD(1);
			g_tSysParas.u32RunCtr += 1;
			MonitorGPS_Per10ms();
			
			u32StateCtr10ms += 1;
			if (u32StateCtr10ms == 300) {
				USBTaskCreate();	// 延时启动USB任务
			}
			
      if ((g_tSysParas.u32Battery < 10) && ((g_bUpdateEnable == true)))
      {
				SetUIPageId(PAGE_WIN, DISP_ID_BATTOFF);
				g_tSysParas.eSysState = SYS_STATE_STANDBY;
      }
			else if ((true == CheckPowerKey()) || (g_tSysParas.bPowerOff == true))
			{
				g_tSysParas.bPowerOff = false;
				SetUIPageId(PAGE_WIN, DISP_ID_POWEROFF);
        g_tSysParas.eSysState = SYS_STATE_STANDBY;
      }

			if (g_tSysParas.eSysState == SYS_STATE_STANDBY)
			{
				SaveWorkParas();
				
				if (g_b4GEnable) {
					TaskNetDelete();
				}
				
				ExtPowerOff();
				BoardTaskDel();
				OSTaskDel(&USBTaskTCB,&err);
				BuzzerKey();
				ModuleVoice(VOICE_SN_PADOWN);
				
				u32StateCtr10ms = 0;
			}
    }
    
    /***************************待机状态*******************************************/
    else if (g_tSysParas.eSysState == SYS_STATE_STANDBY)
    {
			u32StateCtr10ms += 1;
			
			if (u32StateCtr10ms == 150)
			{
				SetUIPageId(PAGE_STANDBY, DISP_ID_NULL);
				POWER_HOLD(0);
			}
			else if (u32StateCtr10ms > 300)
			{
				POWER_HOLD(0);
				
// 				if (g_tSysParas.bChgIn == true) {
// 					SetUIPageId(PAGE_CHG, DISP_ID_NULL);
// 				} else {
// 					SetUIPageId(PAGE_STANDBY, DISP_ID_NULL);
// 				}
				
				if (true == CheckPowerKey())
				{
          g_tSysParas.eSysState = SYS_STATE_VERIFY;
        }
			}
    }
    
    OSTimeDly(10,OS_OPT_TIME_PERIODIC,&err);
  }
}

/*
判断电源键,每10ms调用一次
返回;true = 电源键有动作, false = 电源键无动作
*/
static  BOOL CheckPowerKey(void)
{
  static u32 u32PwkCtr = 0;
  BOOL pwk = false;
  
  if (g_tSysParas.bPwkUp == true) 
  {
    if(Read_PWK()) {
      u32PwkCtr += 1;
      if (g_tSysParas.bScreenShotsEnable == true)
      {
        if (u32PwkCtr >= 200) {
          u32PwkCtr = 0;
          g_tSysParas.bPwkUp = false;
          pwk = true;
        }
      }
      else 
      {
        if (u32PwkCtr >= 50) {
          u32PwkCtr = 0;
          g_tSysParas.bPwkUp = false;
          pwk = true;
        }
      }
    } else {
      if (g_tSysParas.bScreenShotsEnable == true) {
        if (u32PwkCtr > 20) {
          bScreenShots = true;
          ModuleVoice(VOICE_SN_BUZZER);
        }
      }
      
      u32PwkCtr = 0;
    }
  }
  else 
  {
    if(!Read_PWK()) {
      g_tSysParas.bPwkUp = true;
      u32PwkCtr = 0;
    }
  }
  
  return pwk;
}

#if UPDATE_ENABLE > 0 
static const u32 u32LedCnt = 100;
#else
static const u32 u32LedCnt = 50;
#endif

static void MonitorLed(void)
{
	static u32 u32Ctr = u32LedCnt;
	
	u32Ctr -= 1;
	if (u32Ctr == 0)
	{
		u32Ctr = u32LedCnt;
		GPIO_ToggleBits(GPIOE,GPIO_Pin_5);
	}
}

//extern void Debug_gSysParas(void);

// static  void  DataTask  (void *p_arg)
// {
//     OS_ERR      err;

//    (void)p_arg;
//     
// //    srand(OSTimeGet(&err));
//     
//     while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */

//         //Debug_gSysParas();
//         
// //         g_tSysParas.AllocNum.u32FreeBlocks    = GUI_ALLOC_GetNumFreeBlocks();
// //         g_tSysParas.AllocNum.u32FreeBytes     = GUI_ALLOC_GetNumFreeBytes();
// //         g_tSysParas.AllocNum.u32UsedBlocks    = GUI_ALLOC_GetNumUsedBlocks();
// //         g_tSysParas.AllocNum.u32UsedBytes     = GUI_ALLOC_GetNumUsedBytes();
//         
//         OSTimeDlyHMSM(0, 0, 1, 0,
//                       OS_OPT_TIME_HMSM_STRICT,
//                       &err);
//     }
// }

// void task(void) __attribute((section(".ARM.__at_0x8008000")));

// int array[10] __attribute((section(".ARM.__at_0x20001000")));
// int array[10] = {1,2,3,4,5,6,7,8,9,0};

// void task(void)
// {
//     int i;
//     for (i=0;i<10;i++){
//         i = array[i];
//     }
// }

float Volt2Battery(u16 volt)
{
	u16 u16Volt[11] = {4050,3964,3896,3800,3702,3630,3580,3540,3494,3434,3322};
	u08 u08Bat[11] = {100,90,80,70,60,50,40,30,20,10,0};
	u32 i;
	float k,b;

	for (i=0; i<sizeof(u16Volt)/sizeof(u16Volt[0]); i++)
	{
		if (volt >= u16Volt[i])
		{
			break;
		}
	}
	
	if (i == 0)
	{
		return 100;
	}
	else if (i >= sizeof(u16Volt)/sizeof(u16Volt[0]))
	{
		return 0;
	}
	
	k = (float)(u08Bat[i-1] - u08Bat[i]) / (float)(u16Volt[i-1] - u16Volt[i]);
	b = (float)u08Bat[i-1] - k * u16Volt[i-1];
	
	k = k * volt + b;
	
	return k;
}

void MonitorBattery(u32 interval)
{
 	const u32 BattPowerFilterLevel = 5;
 	static u32 BCtr = 0;
 	static float fBattery = 100;
 	float ftemp;
	
	BCtr += 1;
	if (BCtr >= interval)
	{
		BCtr = 0;
		g_tSysParas.u32BattVolt = GetBattAdcVal();
		ftemp = Volt2Battery(g_tSysParas.u32BattVolt);
		if (interval == 1)
		{
			fBattery = ftemp;
		}
		else 
		{
			ftemp = ftemp/BattPowerFilterLevel + fBattery*(BattPowerFilterLevel-1)/BattPowerFilterLevel;
			fBattery = ftemp;
// 			if (ftemp > (fBattery+1) || ftemp < fBattery || ftemp >= 99.5)
// 			{
// 				fBattery = ftemp;
// 			}
		}
		
		g_tSysParas.u32Battery = (u32)(fBattery + 0.5);
		if (g_tSysParas.u32Battery > 100) {
			g_tSysParas.u32Battery = 100;
		}
		
	}
}

void MonitorDCIN(void)
{
// 	static u32 led_ctr = 0;
// 	
// 	if (Read_DC_IN()) {
// 		g_tSysParas.bChgIn = true;
// 		if (g_tSysParas.u32Battery == 100) {
// 			led_ctr += 1;
// 			if (led_ctr < 100) {
// 				LEDR(0); LEDG(1);
// 			} else if (led_ctr < 200) {
// 				LEDR(0); LEDG(0);
// 			} else {
// 				led_ctr = 0;
// 			}
// 		} else {
// 			led_ctr += 1;
// 			if (led_ctr < 100) {
// 				LEDR(1); LEDG(0);
// 			} else if (led_ctr < 200) {
// 				LEDR(0); LEDG(0);
// 			} else {
// 				led_ctr = 0;
// 			}
// 		}
// 	} else {
// 		if (g_tSysParas.bChgIn == true) {
// 			g_tSysParas.bChgIn = false;
// 			LEDR(0); LEDG(0);
// 		}
// 	}
	
	g_tSysParas.bChgIn = false;
}

void EnableBuzzer(u32 freq);
void DisableBuzzer(void);

volatile u32 u32BuzzerFreq = 0;
volatile u32 u32BuzzerCtr = 0;	// 单位 10ms

void BuzzerKey(void)
{
	__disable_irq();
	u32BuzzerFreq = 3000;
	u32BuzzerCtr = 10;
	__enable_irq();
}

void BuzzerLong(void)
{
	__disable_irq();
	u32BuzzerFreq = 1600;
	u32BuzzerCtr = ~0;
	__enable_irq();
}

void BuzzerStop(void)
{
	__disable_irq();
	u32BuzzerFreq = 0;
	__enable_irq();
}

void MonitorBuzzer(void)
{
//	static u32 Freq = 0;
	static u32 Ctr = 0;
	
	if ((u32BuzzerCtr > 0) && (u32BuzzerFreq > 0))
	{
// 		if ((Ctr > 0) && (u32BuzzerFreq == Freq))
// 		{
// 			Ctr = u32BuzzerCtr;
// 		}
// 		else
// 		{
// 			Ctr = u32BuzzerCtr;
// 			Freq = u32BuzzerFreq;
// 			EnableBuzzer(Freq);
// 		}
// 		
// 		Freq = u32BuzzerFreq;
		EnableBuzzer(u32BuzzerFreq);
		Ctr = u32BuzzerCtr;
		u32BuzzerCtr = 0;
	}
	else if (u32BuzzerFreq == 0)
	{
		Ctr = 0;
		DisableBuzzer();
	}
	else if (Ctr > 0)
	{
		Ctr -= 1;
		if (Ctr == 0) {
			DisableBuzzer();
		}
	}
}

// void  MonitorTask  (void *p_arg)   // __attribute((section("MY_CODE")))
// {
//   OS_ERR      err;
//   const u32 BattPowerFilterLevel = 4;
// 	u32 led_ctr = 0;
//   float fBattery = 0;
//   float ftemp;

//   (void)p_arg;

// //  Key_Init();
//   ADC_Batt_Init();

//   while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */

//     OSTimeDly(10,OS_OPT_TIME_PERIODIC,&err);
//     
//     MonitorGPS_Per10ms();

//     /***************************电池电量计算*******************************************/
//     g_tSysParas.u32BattCtr += 1;
//     if (g_tSysParas.u32BattCtr >= g_tSysParas.u32BattCnt)
//     {
//       g_tSysParas.u32BattCtr = 0;
//       ftemp = GetBattAdcVal();
//         g_tSysParas.u32BattVolt = ftemp;
//       if (g_tSysParas.bChgIn == false) {
//         ftemp = 0.05*g_tSysParas.u32BattVolt - 575;   // 11.5V = 0% , 13.5V = 100%
//       } else {
//         ftemp = 0.059*g_tSysParas.u32BattVolt - 738;   // 12.5V = 0% , 14.2V = 100%
//       }
//       
//       if (ftemp < 0) {
//         ftemp = 0;
//       }
//       
//       if (g_tSysParas.u32BattCnt != 1)
//       {
//           ftemp = ftemp/BattPowerFilterLevel + fBattery*(BattPowerFilterLevel-1)/BattPowerFilterLevel;

//           if (g_tSysParas.bChgIn == false) {
//               if ((ftemp > fBattery) && (ftemp < (fBattery+5))) { // 回差
//                 ftemp = fBattery;
//               }
//           } else {
//               if ((ftemp < fBattery) && (ftemp > (fBattery-10))) {
//                   ftemp = fBattery;
//               }
//               if (ftemp < 3) {
//                   ftemp = 3;
//               }
//           }
//       }
//       
//       fBattery = ftemp;
//       g_tSysParas.u32Battery = (u32)(fBattery + 0.5f);
//       if (g_tSysParas.u32Battery > 100) {
//         g_tSysParas.u32Battery = 100;
//       }
//     }

//     /***************************充电LED指示*******************************************/
//     if (Read_DC_IN()) {
//       g_tSysParas.bChgIn = true;
//       if (g_tSysParas.u32Battery == 100) {
//         led_ctr += 1;
//         if (led_ctr < 100) {
//           LEDR(0); LEDG(1);
//         } else if (led_ctr < 200) {
//           LEDR(0); LEDG(0);
//         } else {
//           led_ctr = 0;
//         }
//       } else {
//         led_ctr += 1;
//         if (led_ctr < 100) {
//           LEDR(1); LEDG(0);
//         } else if (led_ctr < 200) {
//           LEDR(0); LEDG(0);
//         } else {
//           led_ctr = 0;
//         }
//       }
//     } else {
//       g_tSysParas.bChgIn = false;
//       LEDR(0); LEDG(0);
//     }
// 		
// 		g_tSysParas.bChgIn = true;

//     /***************************电源保持*******************************************/
//     if (g_tSysParas.bPowerHold == true)
//     {
// //      LEDB(0);
//       if (Read_BAT_VS()) {
//         BAT_VS(0);
//       } else {
//         BAT_VS(1);
//       }
//     }
//     else
//     {
//       BAT_VS(0);
// //       if (g_tSysParas.bChgIn == false)
// //       {
// //         led_ctr += 1;
// //         if (led_ctr < 100) {
// //           LEDB(0);
// //         } else if (led_ctr < 200) {
// //           LEDB(1);
// //         } else {
// //           led_ctr = 0;
// //         }
// //       }
// //       else 
// //       {
// //         LEDB(0);
// //       }
//     }
//     
//     g_tSysParas.u32RunCtr += 1;
//   }
// }

#define W25QX_PAGE_SIZE         0x100
#define W25QX_SECTOR_SIZE       0x1000

#define W25QX_NETPARAS_FLAG      0x25645000

void W25qx_SaveData(u32 Addr, u32 Size, u08 *pByte)
{
  if (g_tSysParas.bW25qxError == true) {
    return ;
  }
  
  if (Size > W25QX_SECTOR_SIZE) {
    Size = W25QX_SECTOR_SIZE;
  }
  
  W25QX_EraseSector(Addr);
  
  while (Size)
  {
    if (Size >= W25QX_PAGE_SIZE) 
    {
      W25QX_WritePage(Addr, pByte, W25QX_PAGE_SIZE);
      Addr += W25QX_PAGE_SIZE;
      pByte += W25QX_PAGE_SIZE;
      Size -= W25QX_PAGE_SIZE;
    }
    else 
    {
      W25QX_WritePage(Addr, pByte, Size);
      Size = 0;
    }
  }
}

static void GetW25qxParas(void)
{
  u32 u32TestSetVal = 0x12345678;
  u32 u32TestGetVal = 0;
  
  W25QX_Init();
	
	W25QX_ReadBytes(W25QX_TEST_ADDR, (u08 *)&u32TestGetVal, sizeof(u32TestGetVal));
  
  if (u32TestGetVal == u32TestSetVal)
  {
    g_tSysParas.bW25qxError = false;
  }
  else 
  {
    W25qx_SaveData(W25QX_TEST_ADDR,sizeof(u32TestSetVal), (u08 *)&u32TestSetVal);
    
    W25QX_ReadBytes(W25QX_TEST_ADDR, (u08 *)&u32TestGetVal, sizeof(u32TestGetVal));
   
    if (u32TestGetVal == u32TestSetVal) {
      g_tSysParas.bW25qxError = false;
    } else {
      g_tSysParas.bW25qxError = true;
    }
  }
	
	g_tNet.b4GEnable = 0;
	g_tNet.u32Addr = W25QX_NETPARAS_ADDR;
  W25QX_ReadBytes(g_tNet.u32Addr, (u08 *)&g_tNet.u32Addr, 8);

	if (g_tNet.u32Flag != W25QX_NETPARAS_FLAG)
  {
    g_tNet.u32Addr = W25QX_NETPARAS_ADDR;
    g_tNet.u32Flag = W25QX_NETPARAS_FLAG;
    W25qx_SaveData(g_tNet.u32Addr,sizeof(g_tNet), (u08 *)&g_tNet);
  }
  else
  {
    W25QX_ReadBytes(g_tNet.u32Addr, (u08 *)&g_tNet, sizeof(g_tNet));
  }
}

volatile u32 g_bNetParasUpdate;

void SaveNetParas(u32 enable, char *pSN)
{
	u08 i,cs = 0;
	CPU_SR_ALLOC();

	CPU_CRITICAL_ENTER();
	g_tNet.b4GEnable = enable;
	strncpy(g_tNet.sn,pSN, sizeof(g_tNet.sn));
	for (i=0; i<11; i++) {
		cs += pSN[i] - '0';
	}
	g_tNet.sn[11] = (cs % 100) / 10 + '0';
	g_tNet.sn[12] = cs % 10 + '0';
	g_tNet.sn[13] = 0;
	g_bNetParasUpdate = 1;
	CPU_CRITICAL_EXIT();
	
	g_tNet.u32Addr = W25QX_NETPARAS_ADDR;
	g_tNet.u32Flag = W25QX_NETPARAS_FLAG;
	W25qx_SaveData(g_tNet.u32Addr,sizeof(g_tNet), (u08 *)&g_tNet);
}

void SaveNetParasPN(char *pN)
{
	if (*pN && strncmp(pN,g_tNet.pn,11))
	{
		strncpy(g_tNet.pn,pN, 11);
		
		g_tNet.u32Addr = W25QX_NETPARAS_ADDR;
		g_tNet.u32Flag = W25QX_NETPARAS_FLAG;
		W25qx_SaveData(g_tNet.u32Addr,sizeof(g_tNet), (u08 *)&g_tNet);
	}
}

char *GetNetID(void)
{
	if (g_tNet.b4GEnable)
	{
		return g_tNet.sn;
	}
	
	return NULL;
}

/*
*********************************************************************************************************
*                                      CREATE APPLICATION TASKS
*
* Description:  This function creates the application tasks.
*
* Arguments  :  none
*
* Returns    :  none
*********************************************************************************************************
*/

static  void  AppTaskCreate (void)
{

}

static void ModuleTaskCreate (void)
{
  OS_ERR err;
  
  OSTaskCreate((OS_TCB     *)&ModuleTaskTCB,                /* Create the start task                                */
               (CPU_CHAR   *)"Module",
               (OS_TASK_PTR ) ModuleTask,
               (void       *) 0,
               (OS_PRIO     ) MODULE_TASK_PRIO,
               (CPU_STK    *)&ModuleTaskStk[0],
               (CPU_STK_SIZE) MODULE_TASK_STK_SIZE / 10,
               (CPU_STK_SIZE) MODULE_TASK_STK_SIZE,
               (OS_MSG_QTY  ) 10u,
               (OS_TICK     ) 0u,
               (void       *) 0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR     *)&err);
}

// static  void  MonitorTaskCreate (void)
// {
//   OS_ERR err;
//   
//   OSTaskCreate((OS_TCB     *)&MonitorTaskTCB,                /* Create the start task                                */
//                (CPU_CHAR   *)"Monitor",
//                (OS_TASK_PTR ) MonitorTask,
//                (void       *) 0,
//                (OS_PRIO     ) MONITOR_TASK_PRIO,
//                (CPU_STK    *)&MonitorTaskStk[0],
//                (CPU_STK_SIZE) MONITOR_TASK_STK_SIZE / 10,
//                (CPU_STK_SIZE) MONITOR_TASK_STK_SIZE,
//                (OS_MSG_QTY  ) 0u,
//                (OS_TICK     ) 0u,
//                (void       *) 0,
//                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
//                (OS_ERR     *)&err);
// }

// static  void  W25QXTaskCreate (void)
// {
//   OS_ERR err;
//   
//   OSTaskCreate((OS_TCB     *)&W25QXTaskTCB,                /* Create the start task                                */
//                (CPU_CHAR   *)"W25QX",
//                (OS_TASK_PTR ) W25QXTask,
//                (void       *) 0,
//                (OS_PRIO     ) W25QX_TASK_PRIO,
//                (CPU_STK    *)&UITaskStk[0],
//                (CPU_STK_SIZE) UI_TASK_STK_SIZE / 10,
//                (CPU_STK_SIZE) UI_TASK_STK_SIZE,
//                (OS_MSG_QTY  ) 0u,
//                (OS_TICK     ) 0u,
//                (void       *) 0,
//                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
//                (OS_ERR     *)&err);
// }

static  void  UITaskCreate (void)
{
  OS_ERR err;
  
  OSTaskCreate((OS_TCB     *)&UITaskTCB,                /* Create the start task                                */
               (CPU_CHAR   *)"UI",
               (OS_TASK_PTR ) UITask,
               (void       *) 0,
               (OS_PRIO     ) UI_TASK_PRIO,
               (CPU_STK    *)&UITaskStk[0],
               (CPU_STK_SIZE) UI_TASK_STK_SIZE / 10,
               (CPU_STK_SIZE) UI_TASK_STK_SIZE,
               (OS_MSG_QTY  ) 0u,
               (OS_TICK     ) 0u,
               (void       *) 0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
               (OS_ERR     *)&err);
}

static  void  BoardTaskCreate (void)
{
  OS_ERR err;
  
  OSTaskCreate((OS_TCB     *)&BoardTaskTCB,                /* Create the start task                                */
               (CPU_CHAR   *)"Board",
               (OS_TASK_PTR ) BoardTask,
               (void       *) 0,
               (OS_PRIO     ) BOARD_TASK_PRIO,
               (CPU_STK    *)&BoardTaskStk[0],
               (CPU_STK_SIZE) BOARD_TASK_STK_SIZE / 10,
               (CPU_STK_SIZE) BOARD_TASK_STK_SIZE,
               (OS_MSG_QTY  ) 10u,
               (OS_TICK     ) 0u,
               (void       *) 0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
               (OS_ERR     *)&err);
}

static  void  USBTaskCreate (void)
{
  OS_ERR err;
  
  OSTaskCreate((OS_TCB     *)&USBTaskTCB,                /* Create the start task                                */
               (CPU_CHAR   *)"USB",
               (OS_TASK_PTR ) USBTask,
               (void       *) 0,
               (OS_PRIO     ) USB_TASK_PRIO,
               (CPU_STK    *)&USBTaskStk[0],
               (CPU_STK_SIZE) USB_TASK_STK_SIZE / 10,
               (CPU_STK_SIZE) USB_TASK_STK_SIZE,
               (OS_MSG_QTY  ) 0u,
               (OS_TICK     ) 0u,
               (void       *) 0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
               (OS_ERR     *)&err);
}

int gUsbIn;

void CopyPulse(void);
void NetCfgUpdate(void);

static  void  USBTask (void *p_arg)
{
  OS_ERR err;
  
  (void)p_arg;
  
  gUsbIn = 0;
  
  USBH_Init(&USB_OTG_Core, USB_OTG_FS_CORE_ID, &USB_Host, &USBH_MSC_cb, &USR_Callbacks);
   
  while (DEF_TRUE)
  {
    USBH_Process(&USB_OTG_Core, &USB_Host);
    
    if (gUsbIn && (USB_Host.gState == HOST_CLASS)) {
//      g_tSysParas.tError.bUsb = false;
			//CopyPulse();
			NetCfgUpdate();
    } else if (gUsbIn){
      gUsbIn = 0;
			USBH_Init(&USB_OTG_Core, USB_OTG_FS_CORE_ID, &USB_Host, &USBH_MSC_cb, &USR_Callbacks);
//      g_tSysParas.tError.bUsb = true;
    }
    
    OSTimeDly(10,OS_OPT_TIME_DLY,&err);
  }
}

/*
*********************************************************************************************************
*                                      CREATE APPLICATION EVENTS
*
* Description:  This function creates the application kernel objects.
*
* Arguments  :  none
*
* Returns    :  none
*********************************************************************************************************
*/

static  void  AppObjCreate (void)
{
}

void IWDG_Init(void)
{
  const u32 LsiFreq = 32000;
  /* Enable the LSI oscillator ************************************************/
  RCC_LSICmd(ENABLE);
  
  /* Wait till LSI is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
  {
  }

  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

  /* IWDG counter clock: LSI/32 */
  IWDG_SetPrescaler(IWDG_Prescaler_32);

  /* Set counter reload value to obtain 4s IWDG TimeOut.
     IWDG counter clock Frequency = LsiFreq/32
     Counter Reload Value = 1s/IWDG counter clock period
                          = 1s / (32/LsiFreq)
                          = LsiFreq/32
   */   
  IWDG_SetReload(LsiFreq/32);

  /* Reload IWDG counter */
  IWDG_ReloadCounter();

  /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
  IWDG_Enable();
}


