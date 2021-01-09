//----------------------------------------------------------------------------
// File: LobbyClient.cpp
//
// Desc: LobbyClient is a simple lobby client.  It displays all registered DirectPlay 
//       applications on the local system.  It allows the 
//       user to launch one or more of these applications using a chosen 
//       service provider.  A launched lobbied application may be told to either 
//       join or host a game.
//
// Copyright (c) 1999-2001 Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#include <winsock.h>
#include <windows.h>
#include <basetsd.h>
//#include <dplay8.h>
// got dplay8.h from SDK 8.1
#include <dplay8.h>
//#include <dplobby8.h>
// got dplobby8.h from SDK 8.1
#include <dplobby8.h>
//#include <dpaddr.h>
//got <dpaddr.h> from SDK 8.1
#include <dpaddr.h>
//#include <dxerr8.h>
#include <dxerr8.h>
#include <cguid.h>
#include <tchar.h>
#include "DXUtil.h"
#include "resource.h"



//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
//#define DPLAY_SAMPLE_KEY        TEXT("Software\\Microsoft\\DirectX DirectPlay Samples")
#define WM_APP_APPDISCONNECTED  (WM_APP + 1)
#define WM_APP_SETSTATUS        (WM_APP + 2)




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
IDirectPlay8Peer*  g_pDP                         = NULL;    // DirectPlay peer object
IDirectPlay8LobbyClient* g_pLobbyClient          = NULL;    // DirectPlay lobby client
HINSTANCE          g_hInst                       = NULL;    // HINST of app
HWND               g_hDlg                        = NULL;    // HWND of main dialog
TCHAR              g_strAppName[256]             = TEXT("LobbyClient");
GUID*              g_pCurSPGuid                  = NULL;    // Currently selected guid
TCHAR              g_strPlayerName[MAX_PATH]     = TEXT("NP123");               // Local player name
TCHAR              g_strSessionName[MAX_PATH]    = TEXT("NPSESSION");              // Session name
TCHAR              g_strPreferredProvider[MAX_PATH] = TEXT("DirectPlay8 TCP / IP Service Provider");        // Provider string
TCHAR              g_strLocalIP[MAX_PATH];                  // Provider string




//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
HRESULT WINAPI   DirectPlayMessageHandler( PVOID pvUserContext, DWORD dwMessageId, PVOID pMsgBuffer );
HRESULT WINAPI   DirectPlayLobbyMessageHandler( PVOID pvUserContext, DWORD dwMessageId, PVOID pMsgBuffer );
//INT_PTR CALLBACK LobbyClientDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
HRESULT          InitDirectPlay();
VOID             FreeConnectSettings( DPL_CONNECTION_SETTINGS* pSettings );
HRESULT          SendMsgToApp( HWND hDlg );
HRESULT          DisconnectFromApp( HWND hDlg );

GUID            getAdapterGUID(DPN_SERVICE_PROVIDER_INFO* pdnAdapterInfoEnum);
HRESULT         InitAdapters();
HRESULT WriteReg(BOOL isHosting, TCHAR shcPath[MAX_PATH]);




__declspec(dllexport) INT Init();
__declspec(dllexport) HRESULT HostSHC(TCHAR playerPort[MAX_PATH], TCHAR playerName[50], TCHAR sessionName[50], TCHAR shcPath[MAX_PATH]);
__declspec(dllexport) HRESULT JoinSHC(TCHAR playerIP[MAX_PATH], TCHAR playerPort[MAX_PATH], TCHAR playerName[50], TCHAR sessionName[50], TCHAR shcPath[MAX_PATH]);
__declspec(dllexport) INT Deinitialize();

GUID* g_spGUID = (GUID*)malloc(sizeof(GUID));
GUID* g_appGUID = (GUID*)malloc(sizeof(GUID));

//__declspec(dllexport) char* test();
//char* test() {
//    char* chr = (char*)malloc(sizeof(char) * strlen("test"));
//    strcpy(chr, "test");
//    return chr;
//}



//////////              Implementation              //////////

 INT Init() {

    HRESULT hr;
    // Init COM so we can use CoCreateInstance
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (FAILED(hr = InitDirectPlay()))
    {
        return DXTRACE_ERR(L"InitDirectPlay", hr);;
    }

    if (FAILED(hr = InitAdapters())) {
        return DXTRACE_ERR(L"EnumAdapters", hr);
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: InitDirectPlay()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT InitDirectPlay()
{
    HRESULT hr;

    // Create and init IDirectPlay8Peer
    if (FAILED(hr = CoCreateInstance(CLSID_DirectPlay8Peer, NULL,
        CLSCTX_INPROC_SERVER,
        IID_IDirectPlay8Peer,
        (LPVOID*)&g_pDP)))
        return DXTRACE_ERR(L"CoCreateInstance", hr);

    if (FAILED(hr = g_pDP->Initialize(NULL, DirectPlayMessageHandler, 0)))
        return DXTRACE_ERR(L"Initialize", hr);

    // Create and init IDirectPlay8LobbyClient
    if (FAILED(hr = CoCreateInstance(CLSID_DirectPlay8LobbyClient, NULL,
        CLSCTX_INPROC_SERVER,
        IID_IDirectPlay8LobbyClient,
        (LPVOID*)&g_pLobbyClient)))
        return DXTRACE_ERR(L"CoCreateInstance", hr);

    if (FAILED(hr = g_pLobbyClient->Initialize(NULL, DirectPlayLobbyMessageHandler, 0)))
        return DXTRACE_ERR(L"Initialize", hr);

    return S_OK;
}

HRESULT InitAdapters() {
    *g_spGUID = CLSID_DP8SP_TCPIP;
    return S_OK;
}


HRESULT WriteReg(BOOL isHosting, TCHAR shcPath[MAX_PATH]) {
    DWORD   dwSize = 0;
    DWORD   dwPrograms = 0;
    DWORD   iProgram;
    BYTE* pData = NULL;

    HRESULT hr = g_pLobbyClient->EnumLocalPrograms(NULL, pData, &dwSize, &dwPrograms, 0);
    if (hr != DPNERR_BUFFERTOOSMALL && FAILED(hr))
        return DXTRACE_ERR(L"EnumLocalPrograms", hr);

    if (dwSize == 0)
    {
        return S_FALSE;
    }

    pData = new BYTE[dwSize];
    if (FAILED(hr = g_pLobbyClient->EnumLocalPrograms(NULL, pData, &dwSize, &dwPrograms, 0)))
        return DXTRACE_ERR(L"EnumLocalPrograms", hr);

    BOOL shcFound = false;
    wchar_t shc[] = L"Stronghold Crusader Extreme";
    DPL_APPLICATION_INFO* pAppInfo = (DPL_APPLICATION_INFO*)pData;
    for (iProgram = 0; iProgram < dwPrograms; iProgram++, pAppInfo++)
    {
        if (wcscmp(pAppInfo->pwszApplicationName, shc) != 0) {
            continue;
        }
        shcFound = true;
        TCHAR strAppName[MAX_PATH];
        DXUtil_ConvertWideStringToGeneric(strAppName, pAppInfo->pwszApplicationName);
        memcpy(g_appGUID, &pAppInfo->guidApplication, sizeof(GUID));
    }
    SAFE_DELETE_ARRAY(pData);


    HRESULT hCreateGuid;
    WCHAR* guidWideString = (WCHAR*)malloc(sizeof(WCHAR)*39);

    if (!shcFound) {
        hCreateGuid = CoCreateGuid(g_appGUID);
        StringFromCLSID(*g_appGUID, &guidWideString);
    }
    else {
        StringFromCLSID(*g_appGUID, &guidWideString);
    }


    char* guidString = (char*)malloc(sizeof(char) * wcslen(guidWideString));
    DXUtil_ConvertWideStringToGeneric(guidString, guidWideString);

    WCHAR* prefix = L"SOFTWARE\\Microsoft\\DirectPlay8\\Applications\\";
    WCHAR* subKey = (WCHAR*)malloc(sizeof(WCHAR) * (wcslen(prefix) + wcslen(guidWideString)));
    wcscpy(subKey, prefix);
    wcscat(subKey, guidWideString);

    char* keyString = (char*)malloc(sizeof(char) * wcslen(subKey));
    DXUtil_ConvertWideStringToGeneric(keyString, subKey);
    free(subKey);
    free(guidWideString);

    HKEY shcKey;
    
    RegCreateKeyEx(HKEY_CURRENT_USER, keyString, 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, NULL, &shcKey, NULL);

    if (isHosting) {
        DXUtil_WriteStringRegKey(shcKey, TEXT("CommandLine"), TEXT("+host +connect +name"));
    }
    else {
        DXUtil_WriteStringRegKey(shcKey, TEXT("CommandLine"), TEXT("+connect +name"));
    }

    DXUtil_WriteStringRegKey(shcKey, TEXT("ApplicationName"), TEXT("Stronghold Crusader Extreme"));
    DXUtil_WriteStringRegKey(shcKey, TEXT("Description"), TEXT("SHC Extreme"));
    DXUtil_WriteStringRegKey(shcKey, TEXT("CurrentDirectory"), TEXT(""));
    DXUtil_WriteStringRegKey(shcKey, TEXT("Guid"), guidString);
    DXUtil_WriteStringRegKey(shcKey, TEXT("ExecutableFilename"), TEXT("Stronghold_Crusader_Extreme.exe"));
    DXUtil_WriteStringRegKey(shcKey, TEXT("LauncherFilename"), TEXT("Stronghold_Crusader_Extreme.exe"));
    DXUtil_WriteStringRegKey(shcKey, TEXT("ExecutablePath"), TEXT(shcPath));
    DXUtil_WriteStringRegKey(shcKey, TEXT("LauncherPath"), TEXT(shcPath));

    RegCloseKey(shcKey);
    free(guidString);
    return S_OK;
}




HRESULT JoinSHC(TCHAR playerIP[MAX_PATH], TCHAR playerPort[MAX_PATH], TCHAR playerName[50], TCHAR sessionName[50], TCHAR shcPath[MAX_PATH]) {
    HRESULT   hr;
    DPNHANDLE hApplication = NULL;

    BOOL bLaunchNew = true;
    GUID* pAppGuid = g_appGUID;

    WriteReg(false, shcPath);

    // Setup the DPL_CONNECT_INFO struct
    DPL_CONNECT_INFO dnConnectInfo;
    ZeroMemory(&dnConnectInfo, sizeof(DPL_CONNECT_INFO));
    dnConnectInfo.dwSize = sizeof(DPL_CONNECT_INFO);
    dnConnectInfo.pvLobbyConnectData = NULL;
    dnConnectInfo.dwLobbyConnectDataSize = 0;
    dnConnectInfo.dwFlags = 0;
    dnConnectInfo.dwFlags |= DPLCONNECT_LAUNCHNEW;
    dnConnectInfo.guidApplication = *pAppGuid;


    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    BOOL bHosting = false;
    BOOL bNoSettings = false;
    IDirectPlay8Address* pHostAddress = NULL;
    IDirectPlay8Address* pDeviceAddress = NULL;


    // Create a host address if connecting to a host, 
    // otherwise keep it as NULL
    if (FAILED(hr = CoCreateInstance(CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER,
        IID_IDirectPlay8Address, (void**)&pHostAddress)))
        return DXTRACE_ERR(L"CoCreateInstance", hr);

    // Set the SP to pHostAddress
    if (FAILED(hr = pHostAddress->SetSP(g_spGUID)))
        return DXTRACE_ERR(L"SetSP", hr);


    // Add the IP address to pHostAddress
    if (_tcslen(playerIP) > 0)
    {
        WCHAR wstrIP[MAX_PATH];
        DXUtil_ConvertGenericStringToWide(wstrIP, playerIP);

        if (FAILED(hr = pHostAddress->AddComponent(DPNA_KEY_HOSTNAME,
            wstrIP, (wcslen(wstrIP) + 1) * sizeof(WCHAR),
            DPNA_DATATYPE_STRING)))
            return DXTRACE_ERR(L"AddComponent", hr);
    }

    // Add the port to pHostAddress
    DWORD dwPort = _ttoi(playerPort);
    if (FAILED(hr = pHostAddress->AddComponent(DPNA_KEY_PORT,
        &dwPort, sizeof(dwPort),
        DPNA_DATATYPE_DWORD)))
        return DXTRACE_ERR(L"AddComponent", hr);


    // Setup the DPL_CONNECTION_SETTINGS
    DPL_CONNECTION_SETTINGS* pSettings = new DPL_CONNECTION_SETTINGS;
    ZeroMemory(pSettings, sizeof(DPL_CONNECTION_SETTINGS));
    pSettings->dwSize = sizeof(DPL_CONNECTION_SETTINGS);
    pSettings->dpnAppDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
    pSettings->dwFlags = 0;
    pSettings->dpnAppDesc.guidApplication = *pAppGuid;
    pSettings->dpnAppDesc.guidInstance = GUID_NULL;
    pSettings->pdp8HostAddress = pHostAddress;
    pSettings->ppdp8DeviceAddresses = new IDirectPlay8Address*;
    pSettings->ppdp8DeviceAddresses[0] = pDeviceAddress;
    pSettings->cNumDeviceAddresses = 1;

    // Set the pSettings->dpnAppDesc.pwszSessionName
    if (_tcslen(sessionName) == 0)
    {
        pSettings->dpnAppDesc.pwszSessionName = NULL;
    }
    else
    {
        WCHAR wstrSessionName[MAX_PATH];
        DXUtil_ConvertGenericStringToWide(wstrSessionName, sessionName);
        pSettings->dpnAppDesc.pwszSessionName = new WCHAR[wcslen(wstrSessionName) + 1];
        wcscpy(pSettings->dpnAppDesc.pwszSessionName, wstrSessionName);
    }

    // Set the pSettings->pwszPlayerName
    if (_tcslen(playerName) == 0)
    {
        pSettings->pwszPlayerName = NULL;
    }
    else
    {
        WCHAR wstrPlayerName[MAX_PATH];
        DXUtil_ConvertGenericStringToWide(wstrPlayerName, playerName);
        pSettings->pwszPlayerName = new WCHAR[wcslen(wstrPlayerName) + 1];
        wcscpy(pSettings->pwszPlayerName, wstrPlayerName);
    }

    dnConnectInfo.pdplConnectionSettings = pSettings;

    ////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////


    hr = g_pLobbyClient->ConnectApplication(&dnConnectInfo, NULL, &hApplication,
        INFINITE, 0);
    if (FAILED(hr))
    {
        if (hr == DPNERR_NOCONNECTION)
        {
            return DXTRACE_ERR(L"There was no waiting application. ", hr);
        }
        else
        {
            return DXTRACE_ERR(L"Connect Application", hr);
        }
    }
    else
    {
        /*TCHAR strBuffer[20];
        wsprintf(strBuffer, TEXT("0x%x"), hApplication);
        int nIndex = (int)SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_ADDSTRING,
            0, (LPARAM)strBuffer);
        SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_SETITEMDATA,
            nIndex, (LPARAM)hApplication);

        if (LB_ERR == SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_GETCURSEL, 0, 0))
            SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_SETCURSEL, 0, 0);*/
    }

    FreeConnectSettings(dnConnectInfo.pdplConnectionSettings);
    return S_OK;
}











HRESULT HostSHC(TCHAR playerPort[MAX_PATH], TCHAR playerName[50], TCHAR sessionName[50], TCHAR shcPath[MAX_PATH]) {
    HRESULT   hr;
    DPNHANDLE hApplication = NULL;

    BOOL bLaunchNew = false;
    BOOL bLaunchNotFound = true;

    GUID* pAppGuid = g_appGUID;
    WriteReg(true, shcPath);

    // Setup the DPL_CONNECT_INFO struct
    DPL_CONNECT_INFO dnConnectInfo;
    ZeroMemory(&dnConnectInfo, sizeof(DPL_CONNECT_INFO));
    dnConnectInfo.dwSize = sizeof(DPL_CONNECT_INFO);
    dnConnectInfo.pvLobbyConnectData = NULL;
    dnConnectInfo.dwLobbyConnectDataSize = 0;
    dnConnectInfo.dwFlags = 0;
    dnConnectInfo.dwFlags |= DPLCONNECT_LAUNCHNEW;
    dnConnectInfo.guidApplication = *pAppGuid;


    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    BOOL bHosting = true;
    BOOL bNoSettings = false;
    IDirectPlay8Address* pHostAddress = NULL;
    IDirectPlay8Address* pDeviceAddress = NULL;


    // Create a device address to specify which device we are using 
    if (FAILED(hr = CoCreateInstance(CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER,
        IID_IDirectPlay8Address, (void**)&pDeviceAddress)))
        return DXTRACE_ERR(L"CoCreateInstance", hr);

    // Set the SP to pDeviceAddress
    if (FAILED(hr = pDeviceAddress->SetSP(g_spGUID)))
        return DXTRACE_ERR(L"SetSP", hr);

    //// Add the adapter to pHostAddress
    //if (FAILED(hr = pDeviceAddress->SetDevice(g_pAdapterGuid)))
    //    return DXTRACE_ERR(L"SetDevice", hr);


    // Add the port to pDeviceAddress
    DWORD dwPort = _ttoi(playerPort);
    if (FAILED(hr = pDeviceAddress->AddComponent(DPNA_KEY_PORT, &dwPort, sizeof(dwPort), DPNA_DATATYPE_DWORD))) {
        return DXTRACE_ERR(L"AddComponent", hr);
    }


    // Setup the DPL_CONNECTION_SETTINGS
    DPL_CONNECTION_SETTINGS* pSettings = new DPL_CONNECTION_SETTINGS;
    ZeroMemory(pSettings, sizeof(DPL_CONNECTION_SETTINGS));
    pSettings->dwSize = sizeof(DPL_CONNECTION_SETTINGS);
    pSettings->dpnAppDesc.dwSize = sizeof(DPN_APPLICATION_DESC);
    pSettings->dwFlags = 0;
    pSettings->dwFlags |= DPLCONNECTSETTINGS_HOST;
    pSettings->dpnAppDesc.guidApplication = *pAppGuid;
    pSettings->dpnAppDesc.guidInstance = GUID_NULL;
    pSettings->pdp8HostAddress = pHostAddress;
    pSettings->ppdp8DeviceAddresses = new IDirectPlay8Address*;
    pSettings->ppdp8DeviceAddresses[0] = pDeviceAddress;
    pSettings->cNumDeviceAddresses = 1;

    // Set the pSettings->dpnAppDesc.pwszSessionName
    if (_tcslen(sessionName) == 0)
    {
        pSettings->dpnAppDesc.pwszSessionName = NULL;
    }
    else
    {
        WCHAR wstrSessionName[MAX_PATH];
        DXUtil_ConvertGenericStringToWide(wstrSessionName, sessionName);
        pSettings->dpnAppDesc.pwszSessionName = new WCHAR[wcslen(wstrSessionName) + 1];
        wcscpy(pSettings->dpnAppDesc.pwszSessionName, wstrSessionName);
    }

    // Set the pSettings->pwszPlayerName
    if (_tcslen(playerName) == 0)
    {
        pSettings->pwszPlayerName = NULL;
    }
    else
    {
        WCHAR wstrPlayerName[MAX_PATH];
        DXUtil_ConvertGenericStringToWide(wstrPlayerName, playerName);
        pSettings->pwszPlayerName = new WCHAR[wcslen(wstrPlayerName) + 1];
        wcscpy(pSettings->pwszPlayerName, wstrPlayerName);
    }

    dnConnectInfo.pdplConnectionSettings = pSettings;

    ////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////


    hr = g_pLobbyClient->ConnectApplication(&dnConnectInfo, NULL, &hApplication,
        INFINITE, 0);
    if (FAILED(hr))
    {
        if (hr == DPNERR_NOCONNECTION)
        {
            return DXTRACE_ERR(L"There was no waiting application. ", hr);
        }
        else
        {
            return DXTRACE_ERR(L"Connect Application", hr);
        }
    }
    else
    {
        /*TCHAR strBuffer[20];
        wsprintf(strBuffer, TEXT("0x%x"), hApplication);
        int nIndex = (int)SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_ADDSTRING,
            0, (LPARAM)strBuffer);
        SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_SETITEMDATA,
            nIndex, (LPARAM)hApplication);

        if (LB_ERR == SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_GETCURSEL, 0, 0))
            SendDlgItemMessage(hDlg, IDC_ACTIVE_CONNECTIONS, LB_SETCURSEL, 0, 0);*/
    }

    FreeConnectSettings(dnConnectInfo.pdplConnectionSettings);
    return S_OK;
}



INT Deinitialize() {
    /*GUID* g_spGUID = (GUID*)malloc(sizeof(GUID));
    GUID* g_appGUID = (GUID*)malloc(sizeof(GUID));
    */

    CoUninitialize();

    free(g_spGUID);
    free(g_appGUID);
    return S_OK;
}








/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// Name: FreeConnectSettings
// Desc: Releases everything involved in a DPL_CONNECTION_SETTINGS struct
//-----------------------------------------------------------------------------
VOID FreeConnectSettings( DPL_CONNECTION_SETTINGS* pSettings )
{
    if( !pSettings )
        return;

    SAFE_DELETE_ARRAY( pSettings->pwszPlayerName ); 
    SAFE_DELETE_ARRAY( pSettings->dpnAppDesc.pwszSessionName );
    SAFE_DELETE_ARRAY( pSettings->dpnAppDesc.pwszPassword );
    SAFE_DELETE_ARRAY( pSettings->dpnAppDesc.pvReservedData );
    SAFE_DELETE_ARRAY( pSettings->dpnAppDesc.pvApplicationReservedData );
    SAFE_RELEASE( pSettings->pdp8HostAddress );
    SAFE_RELEASE( pSettings->ppdp8DeviceAddresses[0] );
    SAFE_DELETE_ARRAY( pSettings->ppdp8DeviceAddresses );
    SAFE_DELETE( pSettings );
}




//-----------------------------------------------------------------------------
// Name: DirectPlayMessageHandler
// Desc: Handler for DirectPlay messages.  This function is called by
//       the DirectPlay message handler pool of threads, so be care of thread
//       synchronization problems with shared memory
//-----------------------------------------------------------------------------
HRESULT WINAPI DirectPlayMessageHandler( PVOID pvUserContext, 
                                         DWORD dwMessageId, 
                                         PVOID pMsgBuffer )
{
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DirectPlayLobbyMessageHandler
// Desc: Handler for DirectPlay lobby messages.  This function is called by
//       the DirectPlay lobby message handler pool of threads, so be careful of 
//       thread synchronization problems with shared memory
//-----------------------------------------------------------------------------
HRESULT WINAPI DirectPlayLobbyMessageHandler( PVOID pvUserContext, 
                                              DWORD dwMessageId, 
                                              PVOID pMsgBuffer )
{
    switch( dwMessageId )
    {
        case DPL_MSGID_DISCONNECT:
        {
            PDPL_MESSAGE_DISCONNECT pDisconnectMsg;
            pDisconnectMsg = (PDPL_MESSAGE_DISCONNECT)pMsgBuffer;

            // We should free any data associated with the 
            // app here, but there is none.

            // Tell the update the UI so show that a application was disconnected
            // Note: The lobby handler must not become blocked on the dialog thread, 
            // since the dialog thread will be blocked on lobby handler thread
            // when it calls ConnectApplication().  So to avoid blocking on
            // the dialog thread, we'll post a message to the dialog thread.
            PostMessage( g_hDlg, WM_APP_APPDISCONNECTED, 0, pDisconnectMsg->hDisconnectId );            
            break;
        }

        case DPL_MSGID_RECEIVE:
        {
            PDPL_MESSAGE_RECEIVE pReceiveMsg;
            pReceiveMsg = (PDPL_MESSAGE_RECEIVE)pMsgBuffer;

            // The lobby app sent us data.  This sample doesn't
            // expected data from the app, but it is useful 
            // for more complex lclients.
            break;
        }

        case DPL_MSGID_SESSION_STATUS:
        {
            PDPL_MESSAGE_SESSION_STATUS pStatusMsg;
            pStatusMsg = (PDPL_MESSAGE_SESSION_STATUS)pMsgBuffer;

            TCHAR* strBuffer = new TCHAR[200];
            wsprintf( strBuffer, TEXT("0x%x: "), pStatusMsg->hSender );
            switch( pStatusMsg->dwStatus )
            {
                case DPLSESSION_CONNECTED:
                    _tcscat( strBuffer, TEXT("Session connected") ); break;
                case DPLSESSION_COULDNOTCONNECT:
                    _tcscat( strBuffer, TEXT("Session could not connect") ); break;
                case DPLSESSION_DISCONNECTED:
                    _tcscat( strBuffer, TEXT("Session disconnected") ); break;
                case DPLSESSION_TERMINATED:
                    _tcscat( strBuffer, TEXT("Session terminated") ); break;
                case DPLSESSION_HOSTMIGRATED:
                	_tcscat( strBuffer, TEXT("Host migrated") ); break;
                case DPLSESSION_HOSTMIGRATEDHERE:
                	_tcscat( strBuffer, TEXT("Host migrated to this client") ); break;

                default:
                {
                    TCHAR strStatus[30];
                    wsprintf( strStatus, TEXT("%d"), pStatusMsg->dwStatus );
                    _tcscat( strBuffer, strStatus );
                    break;
                }
            }

            // Tell the update the UI so show that a application status changed
            // Note: The lobby handler must not become blocked on the dialog thread, 
            // since the dialog thread will be blocked on lobby handler thread
            // when it calls ConnectApplication().  So to avoid blocking on
            // the dialog thread, we'll post a message to the dialog thread.
            PostMessage( g_hDlg, WM_APP_SETSTATUS, 0, (LPARAM) strBuffer );            
            break;
        }

        case DPL_MSGID_CONNECTION_SETTINGS:
        {
            PDPL_MESSAGE_CONNECTION_SETTINGS pConnectionStatusMsg;
            pConnectionStatusMsg = (PDPL_MESSAGE_CONNECTION_SETTINGS)pMsgBuffer;

            // The app has changed the connection settings.  
            // This simple client doesn't handle this, but more complex clients may
            // want to.
            break;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SendMsgToApp
// Desc: Send a dummy message to a connected app for demo purposes 
//-----------------------------------------------------------------------------
HRESULT SendMsgToApp( HWND hDlg )
{
    TCHAR strBuffer[MAX_PATH];
    HRESULT hr;

    int nConnectIndex = (int) SendDlgItemMessage( hDlg, IDC_ACTIVE_CONNECTIONS, LB_GETCURSEL, 0, 0 );
    if( nConnectIndex == LB_ERR )
        return S_OK;

    DPNHANDLE hApplication = (DPNHANDLE) SendDlgItemMessage( hDlg, IDC_ACTIVE_CONNECTIONS, 
                                                             LB_GETITEMDATA, nConnectIndex, 0 );

    // For demonstration purposes, just send a buffer to the app.  This can be used
    // by more complex lobby clients to pass custom information to apps that which
    // can then recieve and process it.
    BYTE buffer[20];
    memset( buffer, 0x03, 20 );
    if( FAILED( hr = g_pLobbyClient->Send( hApplication, buffer, 20, 0 ) ) )
    {
		DXTRACE_ERR(L"Send", hr);
        wsprintf( strBuffer, TEXT("Failure trying to send message to 0x%0.8x."), hApplication );
        MessageBox( NULL, strBuffer, g_strAppName, MB_OK | MB_ICONERROR );
    }
    else
    {
        wsprintf( strBuffer, TEXT("Successfully sent a message to 0x%0.8x."), hApplication );
        MessageBox( NULL, strBuffer, g_strAppName, MB_OK | MB_ICONERROR );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DisconnectFromApp
// Desc: Disconnect from an app
//-----------------------------------------------------------------------------
HRESULT DisconnectFromApp( HWND hDlg )
{
    HRESULT hr;

    int nConnectIndex = (int) SendDlgItemMessage( hDlg, IDC_ACTIVE_CONNECTIONS, LB_GETCURSEL, 0, 0 );
    if( nConnectIndex == LB_ERR )
        return S_OK;

    DPNHANDLE hApplication = (DPNHANDLE) SendDlgItemMessage( hDlg, IDC_ACTIVE_CONNECTIONS, 
                                                             LB_GETITEMDATA, nConnectIndex, 0 );

    if( FAILED( hr = g_pLobbyClient->ReleaseApplication( hApplication, 0 ) ) )
    {
		DXTRACE_ERR(L"LaunchApp", hr);
        MessageBox( NULL, TEXT("Failure trying to disconnect from app. "), 
                    g_strAppName, MB_OK | MB_ICONERROR );
    }

    SendDlgItemMessage( hDlg, IDC_ACTIVE_CONNECTIONS, LB_DELETESTRING, nConnectIndex, 0 );

    return S_OK;
}