
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include "wintypes.h"
#include <dlfcn.h>
#endif

#include "eTPkcs11.h"
#include "eTSAPI.h"

#include <boost/random.hpp> //модуль генератора
#include "iostream"
#include "sstream"
#include "windows.h"
#include "strstream"
#include "string"

static HDC     Prn_hDC;
static long    Prn_FontMetrix;
static long    Prn_LineCtr;
static long    Prn_PrinterOn;
static HFONT   Prn_hFont;
static HFONT   Prn_hFontOld;
static DOCINFO Prn_di;
static LOGFONT Prn_Lf;
static TEXTMETRIC Prn_tm;
static char    Prn_Text[2048];
static char    Prn_Buffer[2048];
static char    Prn_Token_ID[2048];

// prototypes
char *BCX_TmpStr(size_t);
char *mid (char*, int, int=-1);
char *left (char*, int);
char *extract (char*, char*);
char *str (double);
int  PrinterOpen  (void);
void PrinterWrite (char*);
void EjectPage    (void);
void PrinterClose (void);






using  namespace std;




#ifdef _WIN32
#define PKCS11_DLL_NAME		"etpkcs11.dll"
#define ETSAPI_DLL_NAME		"etsapi.dll"
#else
#define PKCS11_DLL_NAME		"libeTPkcs11.so"
#define ETSAPI_DLL_NAME		"libeTSapi.so"
#define LoadLibrary(lib) 	dlopen(lib,RTLD_NOW)
#define GetProcAddress 		dlsym
#define FreeLibrary(lib) 	dlclose(lib)
typedef void * HINSTANCE;
#endif//_WIN32	

#define MAX_ARR_SIZE    1024
 


// circular storage to minimize memory leaks
char *BCX_TmpStr (size_t Bites)
{
  static int StrCnt;
  static char *StrFunc[2048];
  StrCnt = (StrCnt + 1) & 2047;
  if (StrFunc[StrCnt]) free (StrFunc[StrCnt]);
  return StrFunc[StrCnt] = (char*)calloc(Bites+128,sizeof(char));
}


// needed for word wrap feature
char *left (char *S, int length)
{
  register int tmplen = strlen(S);
  char *strtmp = BCX_TmpStr(tmplen);
  strcpy (strtmp,S);
  if (length > tmplen)
    strtmp[tmplen] = 0;
  else
    strtmp[length] = 0;
  return strtmp;
}


// needed for word wrap feature
char *mid (char *S, int start, int length)
{
  register int tmplen = strlen(S);
  char *strtmp;
  if (start > tmplen || start < 1) return BCX_TmpStr(1);
  if (length < 0) length = tmplen - start + 1;
  strtmp = BCX_TmpStr(length);
  strncpy(strtmp,&S [start-1],length);
  strtmp[length] = 0;
  return strtmp;
}


char *extract (char *mane, char *match)
{
  register char *a;
  register char *strtmp = BCX_TmpStr(strlen(mane));
  strcpy(strtmp,mane);
  a = strstr(mane,match);
  if (a) strtmp[a-mane] = 0;
  return strtmp;
}


char *str (double d)
{
  register char *strtmp = BCX_TmpStr(16);
  sprintf(strtmp,"% .15G",d);
  return strtmp;
}


//
// set all the printer options including font
//
int PrinterOpen (void)
{
  int  PointSize = 12;
  char zPrinter[2048];
  
  GetProfileString("WINDOWS","DEVICE","",zPrinter,127);
  // extract up to the comma
  strcpy(zPrinter, (char*)extract(zPrinter,","));
  strcpy(Prn_Text,"Printing ...");
  Prn_hDC = CreateDC("",zPrinter,"",0);
  if (!Prn_hDC) return 0;
  Prn_di.cbSize = sizeof(Prn_di);
  Prn_di.lpszDocName = Prn_Text;
  StartDoc(Prn_hDC,&Prn_di);
  StartPage(Prn_hDC);
  SetTextAlign(Prn_hDC,TA_BASELINE | TA_NOUPDATECP | TA_LEFT);
  SetBkMode(Prn_hDC,TRANSPARENT);
  //
  // Prn_Lf deals with the font, got to play with this, if you
  // want a smaller or larger font size!  Don't forget to change
  // max lines and max char/line in PrinterWrite() accordi! ngly
  //
  Prn_Lf.lfHeight = PointSize*GetDeviceCaps(Prn_hDC,LOGPIXELSY)/72;
  Prn_Lf.lfWidth = 0;
  Prn_Lf.lfEscapement = 0;
  Prn_Lf.lfOrientation = 0;
  Prn_Lf.lfWeight = FW_NORMAL;
  Prn_Lf.lfItalic = 0;
  Prn_Lf.lfUnderline = 0;
  Prn_Lf.lfStrikeOut = 0;
  Prn_Lf.lfCharSet = ANSI_CHARSET;
  Prn_Lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
  Prn_Lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  Prn_Lf.lfQuality = PROOF_QUALITY;
  Prn_Lf.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
  strcpy(Prn_Lf.lfFaceName,TEXT("Sans Serif"));
  Prn_hFont = CreateFontIndirect(&Prn_Lf);
  Prn_hFontOld = (HFONT)SelectObject(Prn_hDC,Prn_hFont);
  GetTextMetrics(Prn_hDC,&Prn_tm);
  Prn_FontMetrix = Prn_Lf.lfHeight;
  Prn_PrinterOn = 1;
  return 1;
}


void PrinterWrite (char *TextIn)
{
  int LPP = 60;  // max line numbers depending on font size
  int CPL = 160;  // max char/line depending on font size
  char sTemp[2048] = {0};
  if (!Prn_PrinterOn)
  {
    MessageBox (GetActiveWindow(),"Printer Problem!","",0);
    return;
  }
  strcpy(sTemp,TextIn);
  while(1)
  {
    // split text if it exceeds max line characters
	 if (strlen(sTemp) > CPL)
    {
      strcpy(Prn_Text, (char*)left(sTemp,CPL));
      strcpy (sTemp, (char*)mid(sTemp,CPL+1));
    }
    else
    {
      strcpy(Prn_Text,sTemp);
      *sTemp = 0;
    }
    Prn_LineCtr++;
    if (Prn_LineCtr >= LPP)
    {
      // max lines exceeded, eject this page for next page
	   EndPage(Prn_hDC);
      Prn_LineCtr = 0;
      StartPage(Prn_hDC);
    }
    TextOut(Prn_hDC,20,Prn_FontMetrix*Prn_LineCtr,Prn_Text,strlen(Prn_Text));
    if(sTemp[0] == 0) break;
  }
}


void PrinterClose (void) 
{
  if (!Prn_PrinterOn) return;
  SelectObject(Prn_hDC,Prn_hFontOld);
  DeleteObject(Prn_hFont);
  EndPage(Prn_hDC);
  EndDoc(Prn_hDC);
  DeleteDC(Prn_hDC);
  Prn_LineCtr = 0;
  Prn_PrinterOn = 0;
}


void EjectPage(void)
{
  EndPage(Prn_hDC);
  Prn_LineCtr = 0;
  StartPage(Prn_hDC);
}




//Генератор сл. чисел
template <typename EngineT, typename DistributionType=int>
struct CRandomT
{
        typedef boost::uniform_int<DistributionType> distribution_type;
        typedef typename distribution_type::result_type result_type;
 
        CRandomT(result_type a, result_type b) : 
        gen_(
                EngineT(static_cast<result_type>(time(NULL))),
                distribution_type(a, b)
                )
        {
        };
        result_type operator()()
        {
                return gen_();
        }
        boost::variate_generator<EngineT, distribution_type> gen_;
};

// Функция выхода 
static void leave(const char * message)
{
  if (message) printf("%s\n", message);
  printf("Press Enter to exit");
  getchar();
  exit(message ? -1 : 0);
}

CK_FUNCTION_LIST_PTR fl = NULL; // PKCS#11 functions list

// Load eTPKCS11 and initialize PKCS#11.
void LoadPKCS11()
{
  HINSTANCE hLib = LoadLibrary(PKCS11_DLL_NAME); 
  if (!hLib) leave("Cannot load eTPkcs11");
  
  CK_C_GetFunctionList f_C_GetFunctionList = NULL;
#ifdef _WIN32
	(FARPROC&)f_C_GetFunctionList= GetProcAddress(hLib, "C_GetFunctionList");
#else
	*(void**)&f_C_GetFunctionList= GetProcAddress(hLib, "C_GetFunctionList");
#endif	
  if (!f_C_GetFunctionList) leave("C_GetFunctionList not found");

  if (CKR_OK != f_C_GetFunctionList(&fl)) leave("C_GetFunctionList failed");
  if (CKR_OK != fl->C_Initialize(0)) leave("C_Initialize failed");
}

// Объявление SAPI функции
typedef CK_RV (*t_SAPI_InitToken)(CK_SLOT_ID, CK_ATTRIBUTE_PTR, CK_ULONG, CK_VOID_PTR, CK_INIT_CALLBACK);
t_SAPI_InitToken _SAPI_InitToken = NULL;

typedef CK_RV (*t_SAPI_GetTokenInfo)(CK_SLOT_ID, CK_ATTRIBUTE_PTR, CK_ULONG);
t_SAPI_GetTokenInfo _SAPI_GetTokenInfo = NULL;


// Load eTSAPI and acquire its usable functions.
void LoadETSAPI()
{
  HINSTANCE hLib = LoadLibrary(ETSAPI_DLL_NAME); 
  if (!hLib) leave("Cannot load eTSapi");

	#ifdef _WIN32
  (FARPROC&)_SAPI_InitToken = GetProcAddress(hLib, "SAPI_InitToken");
	#else
	*(void**)&_SAPI_InitToken = GetProcAddress(hLib, "SAPI_InitToken");
	#endif
  if (!_SAPI_InitToken) leave("SAPI_InitToken not found");


	#ifdef _WIN32
  (FARPROC&)_SAPI_GetTokenInfo = GetProcAddress(hLib, "SAPI_GetTokenInfo");
	#else
	*(void**)&_SAPI_GetTokenInfo = GetProcAddress(hLib, "SAPI_GetTokenInfo");
	#endif
  if (!_SAPI_GetTokenInfo) leave("SAPI_GetTokenInfo not found");

}

// Поиск Етокингов
CK_SLOT_ID LocateToken()
{
  CK_ULONG nSlots = 1;
  CK_SLOT_ID nSlotID;
  if (CKR_OK != fl->C_GetSlotList(TRUE, &nSlotID, &nSlots)) leave("C_GetSlotList failed");
  if (nSlots<1) leave("No eToken inserted");
  return nSlotID;
}

// Прогресс бар иницилизации.
CK_RV Progress(CK_VOID_PTR pContext, CK_ULONG progress)
{
  printf(".");
  return 0;
}

// Find attribute in template based on its type.

void OutName(const char* name) // Print attribute name.
{
  printf("%-30s", name);
}

BOOL CheckEmpty(CK_ATTRIBUTE* attr) // Detect if attribute value is empty.
{
  if (attr->ulValueLen>0) return FALSE;
  printf("<EMPTY>\n");
  return TRUE;
}
CK_ATTRIBUTE* FindAttr(CK_ATTRIBUTE* Template, int nTemplate, CK_ATTRIBUTE_TYPE type)
{
  for (int i=0; i<nTemplate; i++, Template++)
  {
    if (Template->type==type) return Template;
  }
  return NULL;
}
void OutString(CK_ATTRIBUTE* attr, const char* name) // Print attribute value as a string.
{ 
  OutName(name);
  if (CheckEmpty(attr)) return;
  printf("%s\n", (const char*)attr->pValue); 
}
void OutHex(CK_ATTRIBUTE* attr, const char* name)  // Print attribute value as a hexadecimal number.
{ 
  OutName(name);
  if (CheckEmpty(attr)) return;
  printf("%08x\n", *((unsigned int*)attr->pValue)); 
}



int main(int argc, char* argv[]) 
{
  LoadPKCS11();
  LoadETSAPI();
  CK_SLOT_ID nSlotID = LocateToken();
  char product_name[256];
  CK_ULONG token_id;
  CK_RV rv ;
       printf("Usage:\n"
          "  InitToken -init  | for initialization your eToken\n"
          "  Else             | remake PIN and PUK code\n\n");
//----------------------------------------------------------------------------------------------------------
	if (argc==2 && !strcmp(argv[1],"-init"))
	{ 
  //подготовка иницилизации етокена 
  int nMaxRetry = 5; // Retry counter for user password.
  // Template for initialization.
  CK_ATTRIBUTE Template[] = {
	{CKA_SAPI_PIN_SO,       (CK_CHAR_PTR)"012345",    6   				      }, //PUK по умолчанию при иницилизации
 	{CKA_SAPI_RETRY_SO_MAX,     &nMaxRetry,                   sizeof(nMaxRetry) },   //Количество попыток ввода
    {CKA_SAPI_PIN_USER,       (CK_CHAR_PTR)"012345",    6   				      }, //PUK по умолчанию при иницилизации
    {CKA_SAPI_RETRY_USER_MAX, &nMaxRetry,                   sizeof(nMaxRetry) }, //Количество попыток ввода
    {CKA_LABEL,               (CK_CHAR_PTR)"eTokenNAME", 9}, //Так будет называться наш етокинг сейчас
     };

	 printf("Initialization in progress");
 	// Do it!
	 rv = _SAPI_InitToken(nSlotID, Template, sizeof(Template)/sizeof(CK_ATTRIBUTE), NULL, Progress);
	 printf("\n");
	 if (rv!=0) leave("SAPI_InitToken failed - Error Initialization");
	 printf("eToken was successfully initialized\n");
	}
//----------------------------------------------------------------------------------------------------------
	else
	{
	//Тянем инфу из етокена 
		CK_ATTRIBUTE Template_Info[] = 
			{
				{CKA_SAPI_PRODUCT_NAME,        product_name,         sizeof(product_name)},
				{CKA_SAPI_TOKEN_ID,            &token_id,            sizeof(CK_ULONG)},
			};

		if (CKR_OK != _SAPI_GetTokenInfo(nSlotID, Template_Info, sizeof(Template_Info)/sizeof(CK_ATTRIBUTE))) leave("SAPI_GetTokenInfo fail");
		CK_ATTRIBUTE* attr;
	    attr = FindAttr(Template_Info, sizeof(Template_Info)/sizeof(CK_ATTRIBUTE), CKA_SAPI_TOKEN_ID);      
		OutHex(attr, "TOKEN_ID:");
	    sprintf(Prn_Token_ID, "%s %08x","TOKEN ID:", *((unsigned int*)attr->pValue));
  
		// Генерация случайного числа в диапазоне [100000 .. 999999] для Пин и пак кодов
		typedef CRandomT<boost::mt19937> CRandom;
		CRandom rnd(100000, 999999);
		int PIN=rnd();
		int PUK=rnd();
		printf("PIN-Code on user Etoken : %i \n",PIN);
		printf("PUK-Code on SO Etoken : %i \n",PUK);
		//переводим INT to LPCSTR
		char PIN_S[512]={0};
		char PUK_S[512]={0};
		sprintf_s(PIN_S,sizeof(PIN_S),"%i",PIN);
		sprintf_s(PUK_S,sizeof(PUK_S),"%i",PUK);
	  //Кусок кода по смене пин и пак кода
		CK_SESSION_HANDLE hSession;
		rv = fl->C_OpenSession(nSlotID, CKF_RW_SESSION|CKF_SERIAL_SESSION, NULL, NULL, &hSession); //открываем сесию по слотид
		if (rv!=0) leave("C_OpenSession failed");
		rv = fl->C_Login(hSession, CKU_SO, (CK_CHAR*)"012345", 6); //логинемся как SO со стандарным паролем
		if (rv!=0) leave("C_Login failed - Not initialized this program" );
		rv = fl->C_SetPIN(hSession,(CK_CHAR_PTR)"012345",6,(CK_CHAR_PTR)PUK_S,6);  //Меняем So puk код
		if (rv!=0)  leave("SAPI_SetPIN fail - Not remake PUK");
		rv = fl->C_InitPIN(hSession,(CK_CHAR_PTR)PIN_S,6);  //Меняем Pin код из по SO
		if (rv!=0)  leave("SAPI_InitPIN fail - Not remake PIN");
	 //Конец кода по смене PIN and PUK

//Код по авто печати пин конвера
//----------------------------------------------------------------------------------------------------------
		int k;
		printf("Printing PIN an PUK code....\n");
	
		PrinterOpen();
		  for(k = 0; k < 4; k++) PrinterWrite(" ");
		  PrinterWrite("KB NAME (OAO)");
		  //for(k = 0; k < 1; k++) PrinterWrite(" ");
		  PrinterWrite(Prn_Token_ID);
		 //for(k = 0; k < 2; k++) PrinterWrite(" ");
		  PrinterWrite(" ");
			sprintf(Prn_Buffer,"%137s%d","PIN:",PIN);
			PrinterWrite(Prn_Buffer); 
			sprintf(Prn_Buffer,"%137s%d","PUK:",PUK);
			PrinterWrite(Prn_Buffer);
		    PrinterWrite("В данном конверте находится PIN и PUK пароли для работы");
			PrinterWrite("вашего ключа с ЭЦП на носителе - eToken. PUK пароль ");
			PrinterWrite("используется для разблокировки вашего ключа,в случае");
			PrinterWrite("многократного неправильного ввода PIN пароля на eToken.");
		//EjectPage();  // optional to clear out the printer
		PrinterClose();
		cin.get();  // wait

	}
//----------------------------------------------------------------------------------------------------------
  fl->C_Finalize(0);
  leave(NULL);
  return 0;
}

