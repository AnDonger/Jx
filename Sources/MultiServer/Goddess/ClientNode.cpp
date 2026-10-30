#include "stdafx.h"
#include "ClientNode.h"

#include <process.h>
#include <iostream>

#include "IDBRoleServer.h"

#include "Macro.h"

#include "../../../../Headers/KGmProtocol.h"
#include "RoleNameFilter.h"
#include "Utils.h"

using OnlineGameLib::Win32::CCriticalSection;
using OnlineGameLib::Win32::CPackager;
using OnlineGameLib::Win32::CEvent;
using OnlineGameLib::Win32::CBuffer;

HANDLE CClientNode::m_hThread = NULL;
CEvent CClientNode::m_hQuitEvent( NULL, true, false );

extern UINT g_nDBEngineLoop;

CCriticalSection	CClientNode::m_csCL;

CClientNode::stdMap	CClientNode::m_theClientMap;


CClientNode::CDataQueue::CDataQueue( size_t bufferSize /*= 1024 * 64*/, size_t maxFreeBuffers /*= 160*/ )
						: m_theDQAllocator( bufferSize, maxFreeBuffers )
{
}

CClientNode::CDataQueue::~CDataQueue()
{
}

bool CClientNode::CDataQueue::AddData( const BYTE *pData, size_t datalength )
{
	CBuffer *pBuffer = m_theDQAllocator.Allocate();

	pBuffer->AddData( pData, datalength );

	{
		CCriticalSection::Owner lock( m_csQueue );

		m_theData.push_back( pBuffer );
	}

	return true;
}

CBuffer *CClientNode::CDataQueue::Get()
{
	{
		CCriticalSection::Owner lock( m_csQueue );

		if ( !m_theData.empty() )
		{
			CBuffer *pBuffer = m_theData.front();

			pBuffer->AddRef();

			m_theData.pop_front();

			pBuffer->Release();

			return pBuffer;
		}
	}

	return NULL;
}

CClientNode::CClientNode( IServer *pServer, size_t id )
			: m_nIndentity( id ) 
			, m_theAllocator( 1024 * 64, 3 )
			, m_pServer( pServer )
{
	ZeroMemory( m_theProcessArray, sizeof( m_theProcessArray ) );
// Multi Fixed By mrChuCong@gmail.com
	m_theProcessArray[c2s_roleserver_getrolelist] = &CClientNode::_QueryRoleList;
	m_theProcessArray[c2s_roleserver_createroleinfo] = &CClientNode::_CreateRole;
	m_theProcessArray[c2s_roleserver_saveroleinfo] = &CClientNode::_SaveRoleInfo;
	m_theProcessArray[c2s_roleserver_deleteplayer] = &CClientNode::_DelRole;
	m_theProcessArray[c2s_roleserver_getroleinfo] = &CClientNode::_GetRoleInfo;
//	m_theProcessArray[c2s_extend] = &CClientNode::_RelayExtend;
	m_theProcessArray[c2s_gamestatistic] = &CClientNode::_GetGameStat;
	m_theProcessArray[c2s_roleserver_lock] = &CClientNode::_LockOrUnlockRole;
}

CClientNode::~CClientNode()
{
	UnlockAllRole(m_nIndentity);
	SAFE_RELEASE( m_pServer );
}

CClientNode *CClientNode::AddNode( IServer *pServer, size_t id )
{
	CCriticalSection::Owner lock( CClientNode::m_csCL );

	IServer *pCloneServer = NULL;
	pServer->QueryInterface( IID_IIOCPServer, ( void ** )&pCloneServer );

	CClientNode *pNode = new CClientNode( pCloneServer, id );
		
	CClientNode::m_theClientMap.insert( stdMap::value_type( id, pNode ) );

	return pNode;
}

void CClientNode::DelNode( size_t id )
{
	stdMap::iterator it;

	if ( CClientNode::m_theClientMap.end() != ( it = CClientNode::m_theClientMap.find( id ) ) )
	{
		CCriticalSection::Owner lock( CClientNode::m_csCL );

		CClientNode *pNode = ( *it ).second;
	
		CClientNode::m_theClientMap.erase( id );

		SAFE_DELETE( pNode );
	}
}

bool CClientNode::Start( IServer *pServer )
{
	if ( CClientNode::m_hThread == NULL )
	{
		unsigned int threadID = 0;
		CClientNode::m_hQuitEvent.Reset();		
		CClientNode::m_hThread = (HANDLE)::_beginthreadex(0,
			0,
			ThreadFunction,
			( void * )pServer,
			0,
			&threadID );
		
		if ( CClientNode::m_hThread == NULL )
		{
			return false;
		}
	}

	return true;
}

void CClientNode::End()
{
	CClientNode::m_hQuitEvent.Set();

	if ( CClientNode::m_hThread != NULL )
	{
		DWORD result = ::WaitForSingleObject( CClientNode::m_hThread, 50000 );
		
		if ( result == WAIT_TIMEOUT )
		{
			::TerminateThread( CClientNode::m_hThread, ( DWORD )( -2 ) );
		}
		
		if ( CClientNode::m_hThread != NULL )
		{
			::CloseHandle( CClientNode::m_hThread );
			CClientNode::m_hThread = NULL;
		}
	}

	/*
	 * Save all
	 */
}

unsigned int __stdcall CClientNode::ThreadFunction( void *pV )
{
	IServer *pServer = reinterpret_cast< IServer * >( pV );

	ASSERT( pServer );

	try
	{
		while ( !CClientNode::m_hQuitEvent.Wait( 0 ) )
		{
			{
				CCriticalSection::Owner lock( CClientNode::m_csCL );

				CClientNode::stdMap::iterator it;

				for ( it = CClientNode::m_theClientMap.begin();
					it != CClientNode::m_theClientMap.end();
					it ++ )
					{
						CClientNode *pNode = ( CClientNode * )( ( *it ).second );

						ASSERT( pNode );

						pNode->Process();
					}
			}

			if ( ++ g_nDBEngineLoop & 0x80000000 )
			{
				g_nDBEngineLoop = 0;
			}

			if ( g_nDBEngineLoop & 0x1 )
			{
				::Sleep( 1 );
			}
		}
	}
	catch(...)
	{
		::MessageBox( NULL, "CClientNode::ThreadFunction was error!", "Warning", MB_OK );
	}

	return 0L;
}

void CClientNode::AppendData( const void *pData, size_t datalength )
{
	if ( pData && datalength )
	{
		BYTE cProtocol = CPackager::Peek( pData );
		
		if ( cProtocol < s2c_micropackbegin )
		{
			LargePackProcess( pData, datalength );
		}
		else if ( cProtocol > s2c_micropackbegin )
		{
			SmallPackProcess( pData, datalength );
		}
		else
		{
			ASSERT( FALSE && "Error!" );
		}
	}
}

void CClientNode::SmallPackProcess( const void *pData, size_t dataLength )
{
	BYTE cProtocol = CPackager::Peek( pData );
	
	ASSERT( cProtocol >= 0 && cProtocol < s2c_end );

	m_theDataQueue.AddData( ( const BYTE * )pData, dataLength );
}

void CClientNode::LargePackProcess( const void *pData, size_t dataLength )
{
	ASSERT( pData && dataLength );

	CBuffer *pBuffer = m_theRecv.PackUp( pData, dataLength );

	if ( pBuffer )
	{
		m_theDataQueue.AddData( pBuffer->GetBuffer(), pBuffer->GetUsed() );
		
		SAFE_RELEASE( pBuffer );
	}
}

void CClientNode::Process()
{
	CBuffer *pBuffer = m_theDataQueue.Get();

	if ( pBuffer )
	{
		const BYTE *pData = pBuffer->GetBuffer();
		const size_t dataLength = pBuffer->GetUsed();

		BYTE cProtocol = CPackager::Peek( pData );

		if ( cProtocol < c2s_end && m_theProcessArray[cProtocol] )
		{
			( this->*m_theProcessArray[cProtocol] )( ( const void * )pData, dataLength );
		}
	}

	SAFE_RELEASE( pBuffer );
}

void CClientNode::_QueryRoleList( const void *pData, size_t dataLength )
{
	const TProcessData *request = ( const TProcessData * )pData;
	size_t header = sizeof( TProcessData ) - 1;
	if ( dataLength < header + 3 || request->nDataLen != dataLength - header ||
		request->nDataLen > 32 || request->pDataBuffer[0] < 1 || request->pDataBuffer[0] > 3 )
	{
		return;
	}
	char account[32];
	ZeroMemory( account, sizeof( account ) );
	memcpy( account, request->pDataBuffer + 1, request->nDataLen - 1 );
	CBuffer *buffer = m_theAllocator.Allocate();
	TProcessData *response = ( TProcessData * )const_cast< BYTE * >( buffer->GetBuffer() );
	ZeroMemory( response, sizeof( *response ) );
	response->nProtoId = s2c_roleserver_getrolelist_result;
	response->ulIdentity = request->ulIdentity;
	int count = 0;
	size_t recordSize = 0;
	if ( request->bLeave )
	{
		S3DBI_RoleBaseInfoCurrent roles[3];
		count = GetRoleListCurrentOfAccount( account, roles, request->pDataBuffer[0] );
		recordSize = sizeof( roles[0] );
		if ( count > 0 )
		{
			memcpy( response->pDataBuffer + 1, roles, count * recordSize );
		}
	}
	else
	{
		S3DBI_RoleBaseInfo roles[3];
		count = GetRoleListOfAccount( account, roles, request->pDataBuffer[0] );
		recordSize = sizeof( roles[0] );
		if ( count > 0 )
		{
			memcpy( response->pDataBuffer + 1, roles, count * recordSize );
		}
	}
	response->pDataBuffer[0] = count;
	response->nDataLen = 1 + ( count > 0 ? count * recordSize : 0 );
	m_pServer->SendData( m_nIndentity, response, header + response->nDataLen );
	SAFE_RELEASE( buffer );
}


void CClientNode::_CreateRole( const void *pData, size_t dataLength )
{
	if ( !pData || dataLength < sizeof( TProcessData ) - 1 )
	{
		return;
	}
	const TProcessData *request = ( const TProcessData * )pData;
	int result = 0;
	size_t header = sizeof( TProcessData ) - 1;
	if ( dataLength >= sizeof( TProcessData ) + sizeof( TRoleData ) - 1 &&
		request->nDataLen == dataLength - header )
	{
		const TRoleData *role = ( const TRoleData * )( request->pDataBuffer + 1 );
		size_t extra = request->bLeave ? 1 : 0;
		if ( role->dwDataLen >= sizeof( TRoleData ) - 1 &&
			role->dwDataLen + 1 + extra == request->nDataLen &&
			memchr( role->BaseInfo.szName, 0, sizeof( role->BaseInfo.szName ) ) &&
			role->BaseInfo.szName[0] )
		{
			extern CRoleNameFilter g_fltRoleName;
			if ( g_fltRoleName.IsTextPass( role->BaseInfo.szName ) )
			{
				int portrait = request->bLeave ? ( BYTE )request->pDataBuffer[1 + role->dwDataLen] : 0;
				result = SaveRoleInfo( ( char * )role, NULL, TRUE, portrait );
			}
		}
	}
	TProcessData response;
	ZeroMemory( &response, sizeof( response ) );
	response.nProtoId = s2c_roleserver_createrole_result;
	response.ulIdentity = request->ulIdentity;
	response.nDataLen = 1;
	response.pDataBuffer[0] = result == 1 ? 1 : -1;
	m_pServer->SendData( m_nIndentity, &response, sizeof( response ) );
	OnlineGameLib::Win32::TraceNetworkData( "stage=role_create_database result=" +
		OnlineGameLib::Win32::ToString( result ), ( const BYTE * )&response, sizeof( response ) );
}


void CClientNode::_SaveRoleInfo( const void *pData, size_t dataLength )
{
	ASSERT( m_pServer && pData && dataLength );
#ifdef CONSOLE_DEBUG
	cout << "_SaveRoleInfo::Begin" << endl;
#endif

	if ( !pData || dataLength < sizeof( TProcessData ) - 1 + sizeof( TRoleData ) - 1 )
	{
		return;
	}
	TProcessData *pPD = ( TProcessData * )pData;
	TRoleData *pDataRole = ( TRoleData * )pPD->pDataBuffer;
	if ( pPD->nDataLen != dataLength - ( sizeof( TProcessData ) - 1 ) ||
		pDataRole->dwDataLen != pPD->nDataLen ||
		!memchr( pDataRole->BaseInfo.szName, 0, 32 ) )
	{
		return;
	}
	int nResult = 0;
	TRoleData* pRole = (TRoleData*)(&pPD->pDataBuffer[0]);
	char szName[32];
	int len = strlen( pRole->BaseInfo.szName );
	ASSERT( len > 0 );
	len = len > 31 ? 31 : len;
	memcpy( szName, pRole->BaseInfo.szName, len );
	szName[len] = '\0';

	if (IsRoleLockBySelf(szName))
	{
		nResult = SaveRoleInfo( &pPD->pDataBuffer[0], NULL, FALSE );
		if (pPD->bLeave)
		{
			UnlockRoleSelf(szName);
		}
	}
	else
		nResult = 0;

	if ( pPD->ulIdentity >= 0 )
	{
		TProcessData Info;
		
		Info.nProtoId = s2c_roleserver_saverole_result;
		Info.ulIdentity = pPD->ulIdentity;
		Info.nDataLen = 1;
		Info.pDataBuffer[0] = ( nResult == 1 ) ? 1 : -1;
		
		m_pServer->SendData( m_nIndentity, ( const void * )&Info, sizeof( Info ) );
	}

#ifdef CONSOLE_DEBUG
	cout << "_SaveRoleInfo::End" << endl;
#endif
}
void CClientNode::_DelRole( const void *pData, size_t dataLength )
{
	if ( !pData || dataLength < sizeof( TProcessData ) - 1 )
	{
		return;
	}
	const TProcessData *request = ( const TProcessData * )pData;
	bool success = false;
	size_t header = sizeof( TProcessData ) - 1;
	if ( dataLength >= header && request->nDataLen == dataLength - header )
	{
		if ( request->bLeave && request->nDataLen == sizeof( TDBRoleRequestCurrent ) )
		{
			const TDBRoleRequestCurrent *role = ( const TDBRoleRequestCurrent * )request->pDataBuffer;
			if ( memchr( role->szRoleName, 0, 32 ) && memchr( role->szAccountName, 0, 32 ) &&
				!IsRoleLock( ( char * )role->szRoleName ) )
			{
				success = DeleteRole( role->szRoleName, role->szAccountName );
			}
		}
		else if ( !request->bLeave && request->nDataLen > 0 && request->nDataLen < 32 )
		{
			char name[32];
			ZeroMemory( name, sizeof( name ) );
			memcpy( name, request->pDataBuffer, request->nDataLen );
			if ( !IsRoleLock( name ) )
			{
				success = DeleteRole( name );
			}
		}
	}
	TProcessData response;
	ZeroMemory( &response, sizeof( response ) );
	response.nProtoId = s2c_roleserver_deleterole_result;
	response.ulIdentity = request->ulIdentity;
	response.nDataLen = 1;
	response.pDataBuffer[0] = success ? 1 : -1;
	m_pServer->SendData( m_nIndentity, &response, sizeof( response ) );
}


//for Relay System and GM [wxb 2003-7-22]
void CClientNode::_RelayExtend( const void *pData, size_t dataLength )
{
	ASSERT(c2s_extend == *((unsigned char *)pData));
	char* pNewData = ((char *)pData) + 1;
	switch (*((WORD*)pNewData))
	{
	case MAKEWORD(pf_gamemaster, gm_c2s_getrole):
		{
			ASSERT(dataLength == 1 + sizeof(GM_GET_ROLE_DATA_COMMAND));
			GM_GET_ROLE_DATA_COMMAND* pGMData = (GM_GET_ROLE_DATA_COMMAND*)pNewData;
			int nResultBuflen = 0;
			switch(pGMData->wGetID)
			{
			case gm_role_entergame_position:
				{
					ASSERT(0 == pGMData->wLength);
					char* pReturnBuffer = (char*)_alloca(1 + sizeof(GM_GET_ROLE_DATA_SYNC) + sizeof(GM_ROLE_DATA_SUB_ENTER_POS));

					*pReturnBuffer = (char)s2c_extend;

					GM_GET_ROLE_DATA_SYNC* pGMReturn = (GM_GET_ROLE_DATA_SYNC*)(pReturnBuffer + 1);
					strncpy(pGMReturn->AccountName, pGMData->AccountName, sizeof(pGMData->AccountName));
					pGMReturn->ProtocolFamily = pf_gamemaster;
					pGMReturn->ProtocolType = gm_s2c_getrole;
					pGMReturn->wGetID = gm_role_entergame_position;
					pGMReturn->wLength = sizeof(GM_ROLE_DATA_SUB_ENTER_POS);

					GetRoleInfoForGM(pGMData->wGetID, (char*)(pGMReturn + 1), pGMData->AccountName, nResultBuflen);
					ASSERT(pGMReturn->wLength == nResultBuflen);
					m_pServer->SendData( m_nIndentity, pReturnBuffer, 1 + sizeof(GM_GET_ROLE_DATA_SYNC) + sizeof(GM_ROLE_DATA_SUB_ENTER_POS) );	
				}
				break;
			default:
				ASSERT(0);
				break;
			}
		}
		break;

	case MAKEWORD(pf_gamemaster, gm_c2s_setrole):
		{
			ASSERT(dataLength >= 1 + sizeof(GM_SET_ROLE_DATA_COMMAND));
			GM_SET_ROLE_DATA_COMMAND* pGMData = (GM_SET_ROLE_DATA_COMMAND*)pNewData;
			SetRoleInfoForGM(pGMData->wSetID, (char*)(pGMData + 1), pGMData->AccountName, pGMData->wLength);
		}
		break;
	default:
		ASSERT(0);
		break;
	}
}

void CClientNode::_GetRoleInfo( const void *pData, size_t dataLength )
{
	if ( !pData || dataLength < sizeof( TProcessData ) - 1 )
	{
		return;
	}
	const TProcessData *request = ( const TProcessData * )pData;
	char name[32];
	char account[32];
	ZeroMemory( name, sizeof( name ) );
	ZeroMemory( account, sizeof( account ) );
	bool valid = false;
	size_t header = sizeof( TProcessData ) - 1;
	if ( dataLength >= header && request->nDataLen == dataLength - header )
	{
		if ( request->bLeave && request->nDataLen == sizeof( TDBRoleRequestCurrent ) )
		{
			const TDBRoleRequestCurrent *role = ( const TDBRoleRequestCurrent * )request->pDataBuffer;
			if ( memchr( role->szRoleName, 0, 32 ) && memchr( role->szAccountName, 0, 32 ) &&
				role->szAccountName[0] )
			{
				memcpy( name, role->szRoleName, sizeof( name ) );
				memcpy( account, role->szAccountName, sizeof( account ) );
				valid = name[0] != 0;
			}
		}
		else if ( !request->bLeave && request->nDataLen > 0 && request->nDataLen < sizeof( name ) )
		{
			memcpy( name, request->pDataBuffer, request->nDataLen );
			valid = name[0] != 0;
		}
	}
	CBuffer *buffer = m_theAllocator.Allocate();
	TProcessData *response = ( TProcessData * )const_cast< BYTE * >( buffer->GetBuffer() );
	ZeroMemory( response, sizeof( *response ) );
	response->nProtoId = s2c_roleserver_getroleinfo_result;
	response->ulIdentity = request->ulIdentity;
	response->pDataBuffer[0] = -1;
	response->nDataLen = 1;
	if ( valid && !IsRoleLock( name ) )
	{
		int length = 0;
		BYTE portrait = 0;
		GetRoleInfo( ( char * )response->pDataBuffer + 1, name, length, &portrait );
		if ( length > 0 )
		{
			const TRoleData *role = ( const TRoleData * )( response->pDataBuffer + 1 );
			if ( !request->bLeave || !stricmp( role->BaseInfo.caccname, account ) )
			{
				response->pDataBuffer[0] = 1;
				response->nDataLen = length + 1;
				if ( request->bLeave )
				{
					// Current role loading carries portrait metadata outside the saved RoleData ABI.
					response->pDataBuffer[length + 1] = portrait;
					response->nDataLen += sizeof( portrait );
				}
			}
		}
	}
	size_t length = header + response->nDataLen;
	m_theSend.AddData( s2c_roleserver_getroleinfo_result, buffer->GetBuffer(), length, request->ulIdentity );
	CBuffer *pack = m_theSend.GetHeadPack( s2c_roleserver_getroleinfo_result );
	while ( pack )
	{
		m_pServer->SendData( m_nIndentity, pack->GetBuffer(), pack->GetUsed() );
		SAFE_RELEASE( pack );
		pack = m_theSend.GetNextPack( s2c_roleserver_getroleinfo_result );
	}
	m_theSend.DelData( s2c_roleserver_getroleinfo_result );
	SAFE_RELEASE( buffer );
}


void CClientNode::_GetGameStat( const void *pData, size_t dataLength )
{//发送游戏统计数据(By Fellow,2003.7.22)
	ASSERT( m_pServer && pData && dataLength );

#ifdef CONSOLE_DEBUG
	cout << "_GetGameStat::Begin" << endl;
#endif

	//收到的数据包，只有控制字没有数据
	TProcessData *pProData = ( TProcessData * )pData;
	size_t nIdentity = pProData->ulIdentity;

	//需要发送的数据包
	CBuffer *pBuffer = m_theAllocator.Allocate();
	TProcessData *pStatData = 
		reinterpret_cast< TProcessData * >( const_cast< BYTE * >( pBuffer->GetBuffer() ) );

	int nDataLen = sizeof(TGAME_STAT_DATA);
	
	pStatData->nProtoId = s2c_gamestatistic;
	pStatData->ulIdentity = nIdentity;

	pStatData->nDataLen = nDataLen;
		//取得统计数据
	TGAME_STAT_DATA* myGameStatData = (TGAME_STAT_DATA*)pStatData->pDataBuffer;
	GetGameStat(myGameStatData);

	int nUsedLen = sizeof( TProcessData ) - 1 + nDataLen;
	pBuffer->Use( nUsedLen );
	//发送
	
	m_theSend.AddData( s2c_gamestatistic_bigpackage, pBuffer->GetBuffer(), nUsedLen );

	CBuffer *pPack = m_theSend.GetHeadPack( s2c_gamestatistic_bigpackage );

	while ( pPack )
	{
		m_pServer->SendData( m_nIndentity, ( const void * )pPack->GetBuffer(), pPack->GetUsed() );

		SAFE_RELEASE( pPack );

		pPack = m_theSend.GetNextPack( s2c_gamestatistic_bigpackage );
	}

	m_theSend.DelData( s2c_gamestatistic_bigpackage );

	SAFE_RELEASE( pPack );

	SAFE_RELEASE( pBuffer );
	
#ifdef CONSOLE_DEBUG
	cout << "_GetGameStat::End" << endl;
#endif
}

/////////////////////////////////////////////////////////////////////////////////

CClientNode::stdRoleLockMap CClientNode::m_csRoleLock;
CCriticalSection	CClientNode::m_csCR;

void CClientNode::_LockOrUnlockRole( const void *pData, size_t dataLength )	//强行加解锁
{
	CCriticalSection::Owner lock( CClientNode::m_csCR );
	if (pData && dataLength == sizeof(tagRoleEnterGame))
	{
		tagRoleEnterGame* pRole = (tagRoleEnterGame*)pData;
		char szRole[32];
		strncpy(szRole, pRole->Name, 31);
		szRole[31] = 0;
		bool bLock = pRole->bLock;

		if (szRole && szRole[0] != 0)
		{
			if (bLock)
				m_csRoleLock[szRole] = m_nIndentity;
			else
			{
				stdRoleLockMap::iterator it = m_csRoleLock.find(szRole);
				if (it != m_csRoleLock.end())
					m_csRoleLock.erase(it);
			}
		}
	}
}

bool CClientNode::IsRoleLock(char* szRole)
{
	CCriticalSection::Owner lock( CClientNode::m_csCR );
	if (szRole && szRole[0] != 0)
	{
		stdRoleLockMap::iterator it = m_csRoleLock.find(szRole);
		if (it != m_csRoleLock.end() && it->second != -1)
			return true;
	}
	return false;
}

bool CClientNode::IsRoleLockBySelf(char* szRole)
{
	CCriticalSection::Owner lock( CClientNode::m_csCR );
	if (szRole && szRole[0] != 0)
	{
		stdRoleLockMap::iterator it = m_csRoleLock.find(szRole);
		if (it != m_csRoleLock.end() && it->second == m_nIndentity)
			return true;
	}
	return false;
}

bool CClientNode::UnlockRoleSelf(char* szRole)
{
	CCriticalSection::Owner lock( CClientNode::m_csCR );
	if (szRole && szRole[0] != 0)
	{
		stdRoleLockMap::iterator it = m_csRoleLock.find(szRole);
		if (it != m_csRoleLock.end() && it->second == m_nIndentity)
		{
			m_csRoleLock.erase(it);
			return true;
		}
	}
	return false;
}

void CClientNode::UnlockAllRole(size_t ID)
{
	CCriticalSection::Owner lock( CClientNode::m_csCR );
	stdRoleLockMap::iterator it = m_csRoleLock.begin();
	while (it != m_csRoleLock.end())
	{
		if (it->second == ID)
			it->second = -1;
		it++;
	}
}

/////////////////////////////////////////////////////////////////////////////////