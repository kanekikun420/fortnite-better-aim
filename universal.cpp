//d3d11 w2s finder by n7
#include "variables.h"
#include <vector>
#include <d3d11.h>
#include <D3D11Shader.h>
#include <D3Dcompiler.h>//generateshader
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib") //timeGetTime
#include "MinHook/include/MinHook.h" //detour x86&x64
#include "FW1FontWrapper/FW1FontWrapper.h" //font

typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef void(__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__stdcall *D3D11CreateQueryHook) (ID3D11Device* pDevice, const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery);

D3D11PresentHook phookD3D11Present = NULL;
D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11CreateQueryHook phookD3D11CreateQuery = NULL;
ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

IFW1Factory *pFW1Factory = NULL;
IFW1FontWrapper *pFontWrapper = NULL;

#include "main.h" //helper funcs
#include "Utils.h"

//==========================================================================================================================

DWORD WINAPI UpdateThread(LPVOID)
{
	try
	{
		Variables::BaseAddress = (DWORD_PTR)GetModuleHandle(NULL);
		GetModuleInformation(GetCurrentProcess(), (HMODULE)Variables::BaseAddress, &Variables::info, sizeof(Variables::info));
		auto btAddrUWorld = Utils::Pattern::FindPattern((PBYTE)Variables::BaseAddress, Variables::info.SizeOfImage, (PBYTE)"\x48\x8B\x1D\x00\x00\x00\x00\x00\x00\x00\x10\x4C\x8D\x4D\x00\x4C", "xxx???????xxxx?x", 0);
		auto btOffUWorld = *reinterpret_cast< uint32_t* >(btAddrUWorld + 3);
		Variables::m_UWorld = reinterpret_cast< SDK::UWorld** >(btAddrUWorld + 7 + btOffUWorld);

		auto btAddrGObj = Utils::Pattern::FindPattern((PBYTE)Variables::BaseAddress, Variables::info.SizeOfImage, (PBYTE)"\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\xD6", "xxx????x????x????x????xxx", 0);
		auto btOffGObj = *reinterpret_cast< uint32_t* >(btAddrGObj + 3);
		SDK::UObject::GObjects = reinterpret_cast< SDK::FUObjectArray* >(btAddrGObj + 7 + btOffGObj);

		auto btAddrGName = Utils::Pattern::FindPattern((PBYTE)Variables::BaseAddress, Variables::info.SizeOfImage, (PBYTE)"\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0\x75\x50\xB9\x00\x00\x00\x00\x48\x89\x5C\x24", "xxx????xxxxxx????xxxx", 0);
		auto btOffGName = *reinterpret_cast< uint32_t* >(btAddrGName + 3);
		SDK::FName::GNames = *reinterpret_cast< SDK::TNameEntryArray** >(btAddrGName + 7 + btOffGName);

		Utils::Engine::w2sAddress = (DWORD_PTR)Utils::Pattern::FindPattern((PBYTE)Variables::BaseAddress, Variables::info.SizeOfImage, (PBYTE)"\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x81\xEC\x00\x00\x00\x00\x41\x0F\xB6\xF9", "xxxx?xxxx?xxxx????xxxx", 0);
		Utils::Engine::boneAddress = (DWORD_PTR)Utils::Pattern::FindPattern((PBYTE)Variables::BaseAddress, Variables::info.SizeOfImage, (PBYTE)"\x40\x53\x55\x57\x41\x56\x48\x81\xEC\x00\x00\x00\x00\x45\x33\xF6", "xxxxxxxxx????xxx", 0);

		while (true)
		{
			if ((*Variables::m_UWorld) != nullptr)
			{
				Variables::m_persistentLevel = (*Variables::m_UWorld)->PersistentLevel;
				Variables::m_owningGameInstance = (*Variables::m_UWorld)->OwningGameInstance;
				Variables::LocalPlayers = Variables::m_owningGameInstance->LocalPlayers;
				Variables::m_LocalPlayer = Variables::LocalPlayers[0];
				Variables::m_Actors = &Variables::m_persistentLevel->AActors;
				
				SDK::APlayerController* m_PlayerController = Variables::m_LocalPlayer->PlayerController;
				if (m_PlayerController != nullptr)
				{
					SDK::FRotator m_ViewAngles = Variables::m_LocalPlayer->PlayerController->ControlRotation;
					m_ViewAngles = m_PlayerController->ControlRotation;
					wsprintfW(ptrBuf4, ptrData4, (int)(m_ViewAngles.Pitch * 100), (int)(m_ViewAngles.Yaw * 100), (int)(m_ViewAngles.Roll * 100));

					if (m_PlayerController->AcknowledgedPawn != nullptr)
					{

						if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
						{
							SDK::AActor* closestPlayer = (Variables::currentPlayer == nullptr ? Utils::GetClosestPlayer() : Variables::currentPlayer);
							if (closestPlayer != nullptr)
							{
								Variables::currentPlayer = closestPlayer;
								SDK::FVector playerLoc;
								Utils::Engine::GetBoneLocation(static_cast<SDK::ACharacter*>(closestPlayer)->Mesh, &playerLoc, 66);
								Utils::LookAt(m_PlayerController, playerLoc);
							}
						}
						else
						{
							Variables::currentPlayer = nullptr;
						}

						if (m_PlayerController->AcknowledgedPawn->IsA(SDK::AFortPawn::StaticClass()))
						{
							SDK::AFortPawn* m_LocalPawn = static_cast<SDK::AFortPawn*>(m_PlayerController->AcknowledgedPawn);
							if (m_LocalPawn->CurrentWeapon != nullptr)
							{
								if (m_LocalPawn->CurrentWeapon->IsA(SDK::AFortWeaponRanged::StaticClass()))
								{
									SDK::AFortWeaponRanged* m_CurWeapon = static_cast<SDK::AFortWeaponRanged*>(m_LocalPawn->CurrentWeapon);
									m_CurWeapon->CurrentReticleSpread = 0.0f;
									m_CurWeapon->CurrentReticleSpreadMultiplier = 0.0f;
									m_CurWeapon->CurrentStandingStillSpreadMultiplier = 0.0f;
								}
							}
						}
					}
				}
				else
				{
					wsprintfW(ptrBuf4, ptrData4_);
				}
			}

			Variables::isInitialized = true;
			Sleep(1);
		}
	}
	catch (...)
	{
		printf("----------------!!!AN ERROR HAS OCCURRED!!!----------------\n");
	}
	
	return NULL;
}

//==========================================================================================================================

const int MultisampleCount = 1; // Set to 1 to disable multisampling
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){ return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall InitializeHook(LPVOID)
{
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(100);
	} while (!hDXGIDLL);
	Sleep(100);
	CreateThread(NULL, 0, UpdateThread, NULL, 0, NULL);
    return NULL;
}

//==========================================================================================================================

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{ 
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH: // A process is loading the DLL.
		DisableThreadLibraryCalls(hModule);
		GetModuleFileName(hModule, dlldir, 512);
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		CreateThread(NULL, 0, InitializeHook, NULL, 0, NULL);
		break;
	}
	return TRUE;
}
