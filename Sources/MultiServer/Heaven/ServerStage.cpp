#include "ServerStage.h" 
#include <time.h>
#include <process.h>		//Thread define

#include "KSG_EncodeDecode.h"

#include "Cipher.h"
#include "Macro.h"
#include "Exception.h"
#include "SocketAddress.h"
#include "Utils.h"
#include "Win32Exception.h"

using OnlineGameLib::Win32::CIOBuffer;
using OnlineGameLib::Win32::CWin32Exception;

using OnlineGameLib::Win32::_tstring;
using OnlineGameLib::Win32::CCriticalSection;
using OnlineGameLib::Win32::CException;
using OnlineGameLib::Win32::Output;
using OnlineGameLib::Win32::DumpData;
using OnlineGameLib::Win32::GetTimeStamp;
using OnlineGameLib::Win32::ToString;
using OnlineGameLib::Win32::Trace;
using OnlineGameLib::Win32::TraceNetworkData;
using OnlineGameLib::Win32::CSocketAddress;

/*
 * Get server factory interface
 */
STDAPI CreateInterface( REFIID	riid, void	**ppv )
{
	HRESULT hr = E_NOINTERFACE;

	if ( IID_IServerFactory == riid )
	{
		CServerFactory *pObject = new CServerFactory;

		*ppv = reinterpret_cast< void * > ( dynamic_cast< IServerFactory * >( pObject ) );

		if ( *ppv )
		{
			reinterpret_cast< IUnknown * >( *ppv )->AddRef();

			hr = S_OK;
		}
	}

	if ( FAILED( hr ) )
	{
		*ppv = NULL;
	}

	return ( HRESULT )( hr );
}

/*
 * Local per connection data
 */
static LONG gs_lSessionID = 0;

class CPerConnectionData
{
public:
      
	explicit CPerConnectionData( const _tstring &address, WPARAM wParam, const LPARAM &lParam )
         : m_address( address ), m_wParam( wParam ), m_lParam( lParam ),
		   m_lSessionID( ::InterlockedIncrement( &gs_lSessionID ) ), m_lRequestSequence( 0 )
	{
	}

	void SetConnectionFirstParam( WPARAM wParam )
	{
		m_wParam = wParam;
	}

	WPARAM GetConnectionFirstParam() const
	{
		return m_wParam;
	}

	LPARAM GetConnectionSecondParam() const
	{
		return m_lParam;
	}

	_tstring GetConnectionDetails() const
	{
         return m_address;
	}

	LONG GetSessionID() const
	{
		return m_lSessionID;
	}

	LONG NextRequestSequence()
	{
		return ::InterlockedIncrement( &m_lRequestSequence );
	}

private:

	const _tstring	m_address;	
	const LPARAM	m_lParam;

	WPARAM			m_wParam;
	const LONG		m_lSessionID;
	LONG			m_lRequestSequence;

	/*
	 * No copies, do not implement
	 */
	CPerConnectionData( const CPerConnectionData &rhs );
	CPerConnectionData &operator=( const CPerConnectionData &rhs );

};

CIOCPServer::CTaskQueue::~CTaskQueue()
{
	CCriticalSection::Owner locker( m_cs );

	while ( !m_theTaskStack.empty() )
	{
		m_theTaskStack.pop();
	}		
}

void CIOCPServer::CTaskQueue::Push( void *pItem )
{
	CCriticalSection::Owner locker( m_cs );

	m_theTaskStack.push( pItem );
}

void *CIOCPServer::CTaskQueue::Pop()
{
	CCriticalSection::Owner locker( m_cs );

	void *pItem = NULL;
	
	if ( !m_theTaskStack.empty() )
	{
		pItem = m_theTaskStack.top();
		m_theTaskStack.pop();
	}

	return pItem;
}

static unsigned gs_holdrand = time(NULL);

static inline unsigned _Rand()
{
    gs_holdrand = gs_holdrand * 214013L + 2531011L;
     
    return gs_holdrand;
}

static inline void RandMemSet(int nSize, unsigned char *pbyBuffer)
{
    _ASSERT(nSize);
    _ASSERT(pbyBuffer);

    while (nSize--)
    {
        *pbyBuffer++ = (unsigned char)_Rand();
    }
}

CIOCPServer::CIOCPServer( size_t nPlayerMaxCount,
				size_t nPrecision,
				size_t maxFreeSockets,
				size_t maxFreeBuffers,
				size_t maxFreeBuffers_Cache,
				size_t bufferSize_Cache /*= 8192*/,
				size_t bufferSize /*= 1024*/,
				size_t numThreads /*= 0*/ )
		: OnlineGameLib::Win32::CSocketServer( maxFreeSockets, maxFreeBuffers, bufferSize, numThreads )
		, m_theCacheAllocator( bufferSize_Cache, maxFreeBuffers_Cache )
		, m_lRefCount( 0 )
		, m_bGameClientProtocol( false )
		, m_pTlsContext( NULL )
		, m_nPlayerMaxCount( nPlayerMaxCount )
		, m_nPrecision( nPrecision )
		, m_nNetworkBufferMaxLen( bufferSize - 32 /* sizeof( protocol of network ) */ )
		, m_pfnCallBackServerEvent( NULL )
		, m_hQuitHelper( NULL, true, false, NULL /*"EVENT_IOCPSVR_QUIT"*/ )
		, m_hHelperThread( NULL )
{
	/*
	 * m_nNetworkBufferMaxLen > 0
	 */
	ASSERT( !( m_nNetworkBufferMaxLen & 0x80000000 ) );

	size_t index = 0;

	for ( index = 0; index < nPlayerMaxCount; index ++ )
	{
		{
			CCriticalSection::Owner locker( m_csFCN );
			
			//m_freeClientNode.push( index );
			m_freeClientNode.push( index );
		}

		/*
		 * Create a client node and add it into list
		 */
		LPCLIENT_NODE pCN = new CLIENT_NODE;

		pCN->pSocket = NULL;
		pCN->pTls = NULL;
		
		pCN->pRecvBuffer	= m_theCacheAllocator.Allocate();
		pCN->pReadBuffer	= m_theCacheAllocator.Allocate();

		pCN->pWriteBuffer	= m_theCacheAllocator.Allocate();
		
		{
			CCriticalSection::Owner	locker( m_csCM );
			
			m_theClientManager[index] = pCN;
		}
	}

	unsigned int threadID = 0;
	
	m_hHelperThread = (HANDLE)::_beginthreadex( 0, 
								0, 
								HelperThreadFunction,
								( void * )this, 
								0, 
								&threadID );
	
	if ( m_hHelperThread == INVALID_HANDLE_VALUE )
	{
		throw CWin32Exception( _T("CIOCPServer::CIOCPServer() - _beginthreadex"), GetLastError() );
	}
}

CIOCPServer::~CIOCPServer()
{
	
	/*
     * Repeat until stack is empty
	 */
	{
		CCriticalSection::Owner locker( m_csFCN );
		
		while ( !m_freeClientNode.empty() )
		{
			m_freeClientNode.pop();
		}

/*		while ( !m_freeClientNode.empty() )
		{
			m_freeClientNode.pop();
		}
*/
	}

	{
		CCriticalSection::Owner	locker( m_csCM );

		m_usedClientNode.erase( m_usedClientNode.begin(), m_usedClientNode.end() );
		
		CLIENT_MANAGER::iterator it;
		for ( it = m_theClientManager.begin(); it != m_theClientManager.end(); it ++ )
		{
			LPCLIENT_NODE pCN = ( LPCLIENT_NODE )( ( *it ).second );
			
			if ( pCN )
			{
				delete pCN->pTls;
				pCN->pTls = NULL;
				pCN->pRecvBuffer->Release();
				pCN->pReadBuffer->Release();
				
				pCN->pWriteBuffer->Release();

			}
			
			SAFE_DELETE( pCN );
		}
		
		m_theClientManager.erase( m_theClientManager.begin(), m_theClientManager.end() );
	}

	m_hQuitHelper.Set();

	if ( WAIT_TIMEOUT == ::WaitForSingleObject( m_hHelperThread, 5000 ) )
	{
		::TerminateThread( m_hHelperThread, 0L );
	}
	delete m_pTlsContext;
}

STDMETHODIMP CIOCPServer::Startup( )
{
	HRESULT hr = E_FAIL;

	try
	{
		Start();

		hr = S_OK;
	}
	catch( const CException &e )
	{
		Output( _T("CIOCPServer::Startup Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
	}
	catch(...)
	{
		Output( _T("CIOCPServer::Startup - Unexpected exception") );
	}
	
	return hr;	
}

STDMETHODIMP CIOCPServer::Cleanup( )
{
	HRESULT hr = E_FAIL;

	try
	{		
		StopAcceptingConnections();
		
		WaitForShutdownToComplete();

		hr = S_OK;
	}
	catch( const CException &e )
	{
		Output( _T("CIOCPServer::Cleanup Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
	}
	catch(...)
	{
		Output( _T("CIOCPServer::Cleanup - Unexpected exception") );
	}

	return hr;
}

STDMETHODIMP CIOCPServer::OpenService( const unsigned long &ulnAddressToListenOn,
			const unsigned short &usnPortToListenOn )
{
	HRESULT hr = E_FAIL;

	try
	{
		Open( ulnAddressToListenOn, usnPortToListenOn );
		
		StartAcceptingConnections();

		hr = S_OK;
	}
	catch( const CException &e )
	{
		Output( _T("CIOCPServer::OpenService Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
	}
	catch(...)
	{
		Output( _T("CIOCPServer::OpenService - Unexpected exception") );
	}	

	return hr;
}

STDMETHODIMP CIOCPServer::CloseService()
{
	HRESULT hr = E_FAIL;

	try
	{
		StopAcceptingConnections();

		hr = S_OK;
	}
	catch( const CException &e )
	{
		Output( _T("CIOCPServer::CloseService Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
	}
	catch(...)
	{
		Output( _T("CIOCPServer::CloseService - Unexpected exception") );
	}	

	return hr;
}

STDMETHODIMP CIOCPServer::RegisterMsgFilter( LPVOID lpParam, 
		CALLBACK_SERVER_EVENT pfnEventNotify )
{
	m_lpCallBackParam = lpParam;
	m_pfnCallBackServerEvent = pfnEventNotify;

	return S_OK;
}

STDMETHODIMP CIOCPServer::PreparePackSink( )
{

	return S_OK;
}

STDMETHODIMP CIOCPServer::PackDataToClient( const unsigned long &ulnClientID,
			const void * const	pData,
			const size_t		&datalength )
{
	HRESULT hr = E_FAIL;

	if ( ulnClientID < m_nPlayerMaxCount && datalength > 0 )
	{
		LPCLIENT_NODE pCN = NULL;

		CCriticalSection::Owner	locker( m_csCM );

		pCN = ( LPCLIENT_NODE )( m_theClientManager[ulnClientID] );

		if ( pCN && pCN->pSocket )
		{
			try
			{
				CCriticalSection::Owner	lock( pCN->csWriteAction );

				CIOBuffer *pWriteBuffer = pCN->pWriteBuffer;

				const size_t nNetworkBufferLen = pWriteBuffer->GetUsed();

				if ( ( nNetworkBufferLen >= m_nNetworkBufferMaxLen ) || 
					( nNetworkBufferLen + datalength >= m_nNetworkBufferMaxLen ) )
				{
					/*
					 * It need send to client at once, because of buffer is full
					 */
					const BYTE *pPackData = pWriteBuffer->GetBuffer();
					const size_t used = pWriteBuffer->GetUsed();

					_SendDataEx( pCN, pPackData, used );

					pWriteBuffer->Empty();
				}

				pWriteBuffer->AddData( static_cast< const char * const >( pData ), datalength );

				hr = S_OK;
			}
			catch( const CException &e )
			{
				Output( _T("CIOCPServer::PackDataToClient Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );

				pCN->pSocket->Shutdown();
			}
			catch(...)
			{
				Output( _T("CIOCPServer::PackDataToClient - Unexpected exception") );

				pCN->pSocket->Shutdown();
			}			
		}
	}

	return hr;
}

STDMETHODIMP CIOCPServer::SendPackToClient( const unsigned long &ulnClientID /* -1 */ )
{
	HRESULT hr = E_FAIL;

	static const size_t s_len_protocol = sizeof( WORD );

	CCriticalSection::Owner locker( m_csCM );

	LIST::iterator it;
	for ( it = m_usedClientNode.begin(); it != m_usedClientNode.end(); it ++ )
	{
		size_t index = ( size_t )( *it );

		LPCLIENT_NODE pCN = NULL;

		pCN = ( LPCLIENT_NODE )( m_theClientManager[index] );

		if ( pCN && pCN->pSocket )
		{
			CIOBuffer *pBuffer = NULL;
			try
			{
				pBuffer = Allocate();
				
				/*
				* Add data into buffer that it is will be send
				*/
				bool ok = false;

				{
					CCriticalSection::Owner lock( pCN->csWriteAction );
					
					const BYTE * pPackData = pCN->pWriteBuffer->GetBuffer();
					const size_t used = pCN->pWriteBuffer->GetUsed();
					
					if ( used > 0 )
					{
						/*
						 * Add package header
						 */
						const size_t headlength = s_len_protocol + used;
						pBuffer->AddData( reinterpret_cast< const char * >( &headlength ), s_len_protocol );

						pBuffer->AddData( reinterpret_cast< const char * >( pPackData ), used );
						
		                _ASSERT(pCN->uKeyMode == 0);
						KSG_EncodeBuf(
							used,
							(unsigned char *)(pBuffer->GetBuffer() + s_len_protocol),
                            &pCN->uServerKey
						);

						ok = true;
					}
					
					pCN->pWriteBuffer->Empty();
				}
				
				if ( ok )
				{
					WriteTransport( pCN->pSocket, pBuffer->GetBuffer(), pBuffer->GetUsed() );
				}
				
				pBuffer->Release();			
				
				hr = S_OK;
			}
			catch( const CException &e )
			{
				// add by spe 2003/07/06
				if (pBuffer)
				{
					pBuffer->Release();	
					pBuffer = NULL;
				}
				
				Output( _T("CIOCPServer::SendPackToClient Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
				
				pCN->pSocket->Shutdown();
			}
			catch(...)
			{
				// add by spe 2003/07/06
				if (pBuffer)
				{
					pBuffer->Release();	
					pBuffer = NULL;
				}
				
				Output( _T("CIOCPServer::SendPackToClient - Unexpected exception") );
				
				pCN->pSocket->Shutdown();
			}
		}
	}

	return hr;
}

STDMETHODIMP CIOCPServer::SendData( const unsigned long &ulnClientID,
					 const void * const	pData,
					 const size_t		&datalength )
{
	HRESULT hr = E_FAIL;

	static const size_t s_len_protocol = sizeof( WORD );

	if ( NULL == pData || 0 == datalength )
	{
		return hr;
	}

	if ( ulnClientID < m_nPlayerMaxCount )
	{
		LPCLIENT_NODE pCN = NULL;
		
		CCriticalSection::Owner	locker( m_csCM );

		pCN = ( LPCLIENT_NODE )( m_theClientManager[ulnClientID] );

		if ( pCN && pCN->pSocket )
		{
			CIOBuffer *pBuffer = NULL;
			try
			{
				pBuffer = Allocate();
				
				/*
				 * Add package header
				 */
				const size_t headlength = s_len_protocol + datalength;
				pBuffer->AddData( reinterpret_cast< const char * >( &headlength ), s_len_protocol );
				
				/*
				 * Add data into buffer that it is will be send
				 */
				pBuffer->AddData( reinterpret_cast< const char * >( pData ), datalength );

                _ASSERT(pCN->uKeyMode == 0);
				KSG_EncodeBuf(
					datalength,
					(unsigned char *)(pBuffer->GetBuffer() + s_len_protocol),
                    &pCN->uServerKey
				);

				WriteTransport( pCN->pSocket, pBuffer->GetBuffer(), pBuffer->GetUsed() );
				
				pBuffer->Release();			
				
				hr = S_OK;
			}
			catch( const CException &e )
			{
				// add by spe 2003/07/06
				if (pBuffer)
				{
					pBuffer->Release();	
					pBuffer = NULL;
				}

				Output( _T("CIOCPServer::SendPackToClient Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
				
				pCN->pSocket->Shutdown();
			}
			catch(...)
			{
				// add by spe 2003/07/06
				if (pBuffer)
				{
					pBuffer->Release();	
					pBuffer = NULL;
				}

				Output( _T("CIOCPServer::SendPackToClient - Unexpected exception") );
				
				pCN->pSocket->Shutdown();
			}
		}
	}

	return hr;
}

bool CIOCPServer::_SendDataEx( LPCLIENT_NODE pNode,
				 const void * const	pData,
				 const size_t		&datalength )
{
	static const size_t s_len_protocol = sizeof( WORD );

	bool ok = false;

	if ( !pNode || !pData || !datalength )
	{
		return false;
	}
	
	if ( pNode->pSocket )
	{
		CIOBuffer *pBuffer = NULL;
		try
		{
			pBuffer = Allocate();
			
			/*
			* Add package header
			*/
			const size_t headlength = s_len_protocol + datalength;
			pBuffer->AddData( reinterpret_cast< const char * >( &headlength ), s_len_protocol );
			
			/*
			* Add data into buffer that it is will be send
			*/
			pBuffer->AddData( reinterpret_cast< const char * >( pData ), datalength );
			
			_ASSERT(pNode->uKeyMode == 0);
			KSG_EncodeBuf(
				datalength,
				(unsigned char *)(pBuffer->GetBuffer() + s_len_protocol),
				&pNode->uServerKey
				);
			
			WriteTransport( pNode->pSocket, pBuffer->GetBuffer(), pBuffer->GetUsed() );
			
			pBuffer->Release();			
			
			ok = true;
		}
		catch( const CException &e )
		{
			// add by spe 2003/07/06
			if (pBuffer)
			{
				pBuffer->Release();	
				pBuffer = NULL;
			}

			Output( _T("CIOCPServer::_SendDataEx Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
			
			pNode->pSocket->Shutdown();
		}
		catch(...)
		{
			// add by spe 2003/07/06
			if (pBuffer)
			{
				pBuffer->Release();	
				pBuffer = NULL;
			}

			Output( _T("CIOCPServer::_SendDataEx - Unexpected exception") );
			
			pNode->pSocket->Shutdown();
		}
	}

	return ok;
}

STDMETHODIMP_( const void * ) CIOCPServer::GetPackFromClient( const unsigned long &ulnClientID,
			size_t &datalength )
{
	if ( ulnClientID < m_nPlayerMaxCount )
	{
		LPCLIENT_NODE pCN = NULL;

		CCriticalSection::Owner	locker( m_csCM );

		pCN = ( LPCLIENT_NODE )( m_theClientManager[ulnClientID] );

		if ( pCN && pCN->pSocket )
		{
			CCriticalSection::Owner lock( pCN->csReadAction );

			pCN->pReadBuffer->Empty();

			pCN->pRecvBuffer = ProcessDataStream( pCN->pSocket, pCN->pRecvBuffer );

			const char *pData = reinterpret_cast< const char * >( pCN->pReadBuffer->GetBuffer() );
			datalength = pCN->pReadBuffer->GetUsed();

			return pData;
		}
	}

	datalength = 0;
	
	return NULL;
}

STDMETHODIMP CIOCPServer::ShutdownClient( const unsigned long &ulnClientID )
{
	if ( ulnClientID < m_nPlayerMaxCount )
	{
		LPCLIENT_NODE pCN = NULL;

		CCriticalSection::Owner	locker( m_csCM );
			
		pCN = ( LPCLIENT_NODE )( m_theClientManager[ulnClientID] );

		if ( pCN )
		{
			if ( pCN->pSocket )
			{
				pCN->pSocket->AbortiveClose();
			}

			pCN->pReadBuffer->Empty();
			pCN->pRecvBuffer->Empty();

			pCN->pWriteBuffer->Empty();

		}
	}

	return S_OK;
}

STDMETHODIMP_( size_t ) CIOCPServer::GetClientCount( )
{
//	CCriticalSection::Owner locker( m_csCM );
	
	return m_usedClientNode.size();
}

STDMETHODIMP_( const char * ) CIOCPServer::GetClientInfo(
					const unsigned long &ulnClientID )
{

	if ( ulnClientID < m_nPlayerMaxCount )
	{
		LPCLIENT_NODE pCN = NULL;

		CCriticalSection::Owner	locker( m_csCM );

		pCN = ( LPCLIENT_NODE )( m_theClientManager[ulnClientID] );

		if ( pCN && pCN->pSocket )
		{
			const CPerConnectionData *pData = reinterpret_cast< const CPerConnectionData * >( pCN->pSocket->GetUserPtr() );
			
			if ( pData )
			{
				return pData->GetConnectionDetails().c_str();
			}
		}
	}

	return NULL;
}

STDMETHODIMP CIOCPServer::EnableGameClientProtocol()
{
	CCriticalSection::Owner locker( m_csCM );
	if ( !m_usedClientNode.empty() )
	{
		return E_UNEXPECTED;
	}
	m_bGameClientProtocol = true;
	return S_OK;
}

STDMETHODIMP CIOCPServer::QueryInterface( REFIID riid, void** ppv )
{
	/*
	 * By definition all COM objects support the IUnknown interface
	 */
	if( riid == IID_IUnknown )
	{
		AddRef();

		*ppv = static_cast< IUnknown * >( static_cast< IServer * >( this ) );
	}
	else if ( riid == IID_IClientTls )
	{
		AddRef();
		*ppv = static_cast< IClientTls * >( this );
	}
	else if ( riid == IID_IGameClientProtocol )
	{
		AddRef();
		*ppv = static_cast< IGameClientProtocol * >( this );
	}
	else if ( riid == IID_IIOCPServer )
	{
		AddRef();

		*ppv = dynamic_cast< IServer * >( this );	
	}
	else
	{
		*ppv = NULL;

		return E_NOINTERFACE;
	}

	return S_OK;
}
    
STDMETHODIMP_(ULONG) CIOCPServer::AddRef()
{
	return ::InterlockedIncrement( &m_lRefCount );
}

STDMETHODIMP_(ULONG) CIOCPServer::Release()
{
	if ( ::InterlockedDecrement( &m_lRefCount ) > 0 )
	{
		return m_lRefCount;
	}

	delete this;
	return 0L;
}

void CIOCPServer::OnConnectionEstablished( Socket *pSocket,
			 OnlineGameLib::Win32::CIOBuffer *pAddress )
{
	if ( pSocket )
	{
		pSocket->AddRef();

		/*
		 * Allocate per connection data
		 */
		CSocketAddress address( reinterpret_cast< const sockaddr * >( pAddress->GetBuffer() ) );
		
		CPerConnectionData *pData = new CPerConnectionData( address.GetAsString(), ( WPARAM )( -1 ), ( LPARAM )( pSocket ) );
		
		ASSERT( pData );
		
		m_theAddClientQueue.Push( ( void * )pData );
	}
}

bool CIOCPServer::OnConnectionClosing( Socket *pSocket )
{
	if ( pSocket )
	{
		pSocket->Close();
	}

	return true;
}

void CIOCPServer::OnConnectionClosed( Socket *pSocket )
{	
	if ( pSocket )
	{
		TraceRequest( pSocket, _T("disconnect"), NULL, 0, 0, -1, _T("connection_closed") );
		pSocket->AddRef();

		/*
		 * Release per connection data
		 */
		CPerConnectionData *pData = reinterpret_cast< CPerConnectionData * >( pSocket->GetUserPtr() );

		m_theDelClientQueue.Push( pData );
	}
}

void CIOCPServer::ReadCompleted( Socket *pSocket, CIOBuffer *pBuffer )
{
	try
	{
		TraceRequest( pSocket, _T("receive"), pBuffer->GetBuffer(), pBuffer->GetUsed(),
			pBuffer->GetSequenceNumber(), -1, _T("raw_fragment") );

		CCriticalSection::Owner locker( m_csCM );
		CPerConnectionData *pData = reinterpret_cast< CPerConnectionData * >( pSocket->GetUserPtr() );
		if ( !pData || pData->GetConnectionFirstParam() >= m_nPlayerMaxCount )
		{
			pSocket->Shutdown();
			return;
		}
		LPCLIENT_NODE pCN = m_theClientManager[pData->GetConnectionFirstParam()];
		if ( !pCN || pCN->pSocket != pSocket )
		{
			pSocket->Shutdown();
			return;
		}

		const BYTE *pPlain = pBuffer->GetBuffer();
		size_t plainLength = pBuffer->GetUsed();
		std::vector< BYTE > plain;
		if ( pCN->pTls )
		{
			bool wasReady = pCN->pTls->IsReady();
			std::vector< BYTE > wire;
			bool ok = pCN->pTls->Receive( pPlain, plainLength, plain, wire );
			if ( !wire.empty() && !WriteWire( pSocket, &wire[0], wire.size() ) )
			{
				return;
			}
			if ( !ok )
			{
				TraceRequest( pSocket, _T("tls_close"), NULL, 0, 0, -1,
					pCN->pTls->GetError().c_str() );
				pSocket->Shutdown();
				return;
			}
			if ( !wasReady && pCN->pTls->IsReady() )
			{
				TraceRequest( pSocket, _T("tls_ready"), NULL, 0, 0, -1,
					pCN->pTls->GetDescription().c_str() );
			}
			pPlain = plain.empty() ? NULL : &plain[0];
			plainLength = plain.size();
			TraceRequest( pSocket, _T("tls_plain"), pPlain, plainLength,
				pBuffer->GetSequenceNumber(), -1, _T("application_fragment") );
		}

		CCriticalSection::Owner readLock( pCN->csReadAction );
		while ( plainLength )
		{
			size_t available = pCN->pRecvBuffer->GetSize() - pCN->pRecvBuffer->GetUsed();
			if ( !available )
			{
				TraceRequest( pSocket, _T("reject"), NULL, 0, 0, -1, _T("receive_cache_full") );
				pSocket->Shutdown();
				return;
			}
			size_t length = plainLength < available ? plainLength : available;
			pCN->pRecvBuffer->AddData( pPlain, length );
			pPlain += length;
			plainLength -= length;
			if ( pSocket->GetMessageCallback() )
			{
				ProcessMessage( pSocket, pCN->pRecvBuffer );
			}
		}
		pBuffer->Empty();
		pSocket->Read( pBuffer );
	}
	catch( const CException &e )
	{
		Output( _T("ReadCompleted - Exception - ") + e.GetWhere() + _T(" - ") + e.GetMessage() );
		TraceRequest( pSocket, _T("reject"), pBuffer->GetBuffer(), pBuffer->GetUsed(),
			0, -1, e.GetMessage().c_str() );
		pSocket->Shutdown();
	}
	catch(...)
	{
		TraceRequest( pSocket, _T("reject"), pBuffer->GetBuffer(), pBuffer->GetUsed(),
			0, -1, _T("receive_exception") );
		pSocket->Shutdown();
	}
}

CIOBuffer *CIOCPServer::ProcessDataStream( Socket *pSocket, CIOBuffer *pBuffer)
{
	const size_t used = pBuffer->GetUsed();
	
	if ( used >= sizeof( WORD ) )
	{
		const size_t messageSize = GetMessageSize( pBuffer );
		
		if ( messageSize < GetMinimumMessageSize() || messageSize > pBuffer->GetSize() )
		{
			TraceRequest( pSocket, _T("reject"), pBuffer->GetBuffer(), used,
				0, -1, _T("invalid_frame_length") );
			pBuffer->Empty();
			pSocket->Shutdown();
		}
		else if ( used == messageSize )
		{
			Output( _T("Got a complete message and begin to process it") );
			
			/*
			* we have a whole, distinct, message
			*/				
			ProcessCommand( pSocket, pBuffer );
			
			pBuffer->Empty();
		}
		else if ( used > messageSize )
		{
			Output( _T("Got message plus extra data") );
			
			/*
			* we have a message, plus some more data
			* allocate a new buffer, copy the extra data into it and try again
			*/
			
			CIOBuffer *pMessage = pBuffer->SplitBuffer( messageSize );
			
			ProcessCommand( pSocket, pMessage );
			
			pMessage->Release();			
		}
	}
	
	/*
	 * not enough data in the buffer, reissue a read into the same buffer to collect more data
	 */
	return pBuffer;
}

size_t CIOCPServer::GetMinimumMessageSize()
{
	static size_t length = sizeof( WORD ) + sizeof( BYTE );

	/*
     * The smallest possible command we accept is a byte onlye package
	 */
	return length;
}

size_t CIOCPServer::GetMessageSize( const CIOBuffer *pBuffer )
{
	WORD wHeadLen = 0;
	if ( pBuffer->GetUsed() >= sizeof( wHeadLen ) )
	{
		memcpy( &wHeadLen, pBuffer->GetBuffer(), sizeof( wHeadLen ) );
	}

	return ( size_t )( wHeadLen );
}

void CIOCPServer::ProcessCommand( Socket *pSocket, const CIOBuffer *pBuffer )
{
	static const size_t s_len_protocol = sizeof( WORD );

	const BYTE *pData = pBuffer->GetBuffer();   
	const size_t used = pBuffer->GetUsed();

	ASSERT( used > s_len_protocol );

	if ( used <= s_len_protocol )
	{
        return;
	}

	const CPerConnectionData *pPCD = reinterpret_cast< const CPerConnectionData * >( pSocket->GetUserPtr() );

	if ( !pPCD )
	{
		return;
	}

	DWORD dwIndex = pPCD->GetConnectionFirstParam();
	LPCLIENT_NODE pCN = NULL;
	{
		CCriticalSection::Owner	locker( m_csCM );

		if ( NULL == ( pCN = ( LPCLIENT_NODE )( m_theClientManager[dwIndex] ) ) )
		{
            return;
		}
		
		LONG sequence = const_cast< CPerConnectionData * >( pPCD )->NextRequestSequence();
		TraceRequest( pSocket, _T("frame"), pData, used, sequence, -1, _T("wire_complete") );

        // Add by Freeway Chen
        _ASSERT(pCN->uKeyMode == 0);

        KSG_DecodeBuf(
            used - s_len_protocol,
            const_cast<unsigned char *>(pData + s_len_protocol),
            &(pCN->uClientKey)
        );

		TraceRequest( pSocket, _T("request"), pData + s_len_protocol, used - s_len_protocol,
			sequence, pData[s_len_protocol], _T("decoded_poll") );
		pCN->pReadBuffer->AddData( ( const BYTE * )( pData + s_len_protocol ), used - s_len_protocol );
	}
}

void CIOCPServer::TraceRequest( Socket *pSocket, LPCTSTR pStage, const BYTE *pData,
		size_t dataLength, size_t sequence, int protocol, LPCTSTR pDetail )
{
	try
	{
		const CPerConnectionData *pConnection = reinterpret_cast< const CPerConnectionData * >( pSocket->GetUserPtr() );
		_tstring message = _T("stage=") + _tstring( pStage ) + _T(" server=") + ToString( this );
		if ( pConnection )
		{
			message += _T(" session=") + ToString( pConnection->GetSessionID() );
			message += _T(" slot=") + ToString( pConnection->GetConnectionFirstParam() );
			_tstring peer = pConnection->GetConnectionDetails();
			size_t position = peer.find( _T(' ') );
			while ( position != _tstring::npos )
			{
				peer.erase( position, 1 );
				position = peer.find( _T(' ') );
			}
			message += _T(" peer=") + peer;
		}
		message += _T(" sequence=") + ToString( sequence );
		message += _T(" protocol=") + ToString( protocol );
		message += _T(" detail=") + _tstring( pDetail );
		TraceNetworkData( message, pData, dataLength );
	}
	catch( ... )
	{
		::OutputDebugString( _T("CIOCPServer::TraceRequest: logging failed\n") );
	}
}

CServerFactory::CServerFactory()
		: m_lRefCount( 0 ),
		m_nPlayerMaxCount( 0 ),
		m_nPrecision( 0 ),
		m_maxFreeBuffers_Cache( 0 ),
		m_bufferSize_Cache( 0 )
{

}

CServerFactory::~CServerFactory()
{

}

STDMETHODIMP CServerFactory::SetEnvironment( const size_t &nPlayerMaxCount,
			const size_t &nPrecision,
			const size_t &maxFreeBuffers_Cache,
			const size_t &bufferSize_Cache )
{
	m_nPlayerMaxCount = nPlayerMaxCount;
	m_nPrecision = nPrecision;

	m_maxFreeBuffers_Cache = maxFreeBuffers_Cache;
	m_bufferSize_Cache = bufferSize_Cache;

	return S_OK;
}

STDMETHODIMP CServerFactory::CreateServerInterface( REFIID riid, void** ppv )
{
	HRESULT hr = E_NOINTERFACE;

	if ( IID_IIOCPServer == riid )
	{		
		const size_t maxPlayerCount = ( m_nPlayerMaxCount > 0 ) ? m_nPlayerMaxCount : 100;
		const size_t precision = ( m_nPrecision > 0 ) ? m_nPrecision : 10;

		const size_t maxFreeBuffers_Cache = ( m_maxFreeBuffers_Cache > 0 ) ? m_maxFreeBuffers_Cache : 10;
		const size_t bufferSize_Cache = ( m_bufferSize_Cache > 0 ) ? m_bufferSize_Cache : 8192;

		const size_t maxFreeBuffers = 10240;

		try
		{
			CIOCPServer *pObject = new CIOCPServer( maxPlayerCount, 
											precision, 
											maxPlayerCount,
											maxFreeBuffers,
											maxFreeBuffers_Cache,
											bufferSize_Cache );
			
			*ppv = reinterpret_cast< void * > ( dynamic_cast< IServer * >( pObject ) );
			
			if ( *ppv )
			{
				reinterpret_cast< IUnknown * >( *ppv )->AddRef();
				
				hr = S_OK;
			}
		}
		catch( ... )
		{
			TRACE( "CServerFactory::CreateServerInterface exception!" );
		}
	}

	if ( FAILED( hr ) )
	{
		*ppv = NULL;
	}

	return ( HRESULT )( hr );
}

STDMETHODIMP CServerFactory::QueryInterface( REFIID riid, void** ppv )
{
	/*
	 * By definition all COM objects support the IUnknown interface
	 */
	if( riid == IID_IUnknown )
	{
		AddRef();

		*ppv = dynamic_cast< IUnknown * >( this );
	}
	else if ( riid == IID_IServerFactory )
	{
		AddRef();

		*ppv = dynamic_cast< IServerFactory * >( this );	
	}
	else
	{
		*ppv = NULL;

		return E_NOINTERFACE;
	}

	return S_OK;
}
    
STDMETHODIMP_(ULONG) CServerFactory::AddRef()
{
	return ::InterlockedIncrement( &m_lRefCount );
}

STDMETHODIMP_(ULONG) CServerFactory::Release()
{
	if ( ::InterlockedDecrement( &m_lRefCount ) > 0 )
	{
		return m_lRefCount;
	}

	delete this;
	return 0L;
}

unsigned int __stdcall CIOCPServer::HelperThreadFunction( void *pParam )
{
	CIOCPServer *pThis = ( CIOCPServer * )( pParam );

	ASSERT( pThis );

	try
	{
		pThis->_HelperThreadFunction();
	}
	catch( ... )
	{
		TRACE( "CIOCPServer::HelperThreadFunction exception!" );
	}

	return 0L;
}

unsigned int CIOCPServer::_HelperThreadFunction()
{
	bool bAdd, bDel;
	DWORD lastTlsCheck = GetTickCount();

	while ( !m_hQuitHelper.Wait( 0 ) )
	{
		if ( GetTickCount() - lastTlsCheck >= 1000 )
		{
			CheckTlsTimeouts();
			lastTlsCheck = GetTickCount();
		}
		bAdd = _HelperAddClient();

		bDel = _HelperDelClient();

		if ( !bAdd && !bDel )
		{
			::Sleep( 1 );
		}
	}

	return 0L;
}

bool CIOCPServer::_HelperAddClient()
{
   	static const size_t s_len_protocol = sizeof( WORD );

	CPerConnectionData *pData = ( CPerConnectionData * )( m_theAddClientQueue.Pop() );

	if ( NULL == pData )
	{
		return false;
	}

	Socket *pSocket = ( Socket * )( pData->GetConnectionSecondParam() );

	ASSERT( pSocket );
	
	USES_CONVERSION;

	DWORD dwIndex = (DWORD)( -1 );

	if ( pSocket )
	{
		/*
		 * Get ID from the free table
		 */
		{
			CCriticalSection::Owner locker( m_csFCN );

			if ( !m_freeClientNode.empty() )
			{
				dwIndex = m_freeClientNode.front();
				m_freeClientNode.pop();
				//dwIndex = m_freeClientNode.top();
				//m_freeClientNode.pop();
			}
		}

		pData->SetConnectionFirstParam( dwIndex );

		pSocket->SetUserPtr( pData );

		/*
		 * Store this socket if we can use it
		 */
		if ( dwIndex != (DWORD)( -1 ) )
		{
			/*
			 * Store socket into array
			 */
			{
				CCriticalSection::Owner	locker( m_csCM );
				
				m_theClientManager[dwIndex]->pSocket    = pSocket;
				if ( m_pTlsContext )
				{
					m_theClientManager[dwIndex]->pTls = new CClientTlsConnection( *m_pTlsContext );
				}
                m_theClientManager[dwIndex]->uServerKey = _Rand();
                m_theClientManager[dwIndex]->uClientKey = _Rand();
                m_theClientManager[dwIndex]->uKeyMode   = 0;

				/*
				 * Set ID into the used table
				 */
				m_usedClientNode.push_back( dwIndex );
			}

            #pragma pack(1)
            struct  {
                WORD wLen;
                ACCOUNT_BEGIN AccountBegin;
            } SendAccountBegin;
            #pragma pack()

            RandMemSet(sizeof(SendAccountBegin), (unsigned char *)&SendAccountBegin);

			{
				CCriticalSection::Owner	locker( m_csCM );

                SendAccountBegin.wLen = s_len_protocol + sizeof(SendAccountBegin.AccountBegin);

                SendAccountBegin.AccountBegin.ProtocolType = CIPHER_PROTOCOL_TYPE;
                SendAccountBegin.AccountBegin.Mode         = m_theClientManager[dwIndex]->uKeyMode;
                SendAccountBegin.AccountBegin.ServerKey    = ~m_theClientManager[dwIndex]->uServerKey;
                SendAccountBegin.AccountBegin.ClientKey    = ~m_theClientManager[dwIndex]->uClientKey;
				
			}

			if ( m_bGameClientProtocol )
			{
				// Verified in Release/Client: 0x42BD30, 0x42BD80, table 0x99CB14.
				static const unsigned uGameKeyTable[10] =
				{
					0x6FEE50A2, 0x7099AB45, 0x70467002, 0x6FCE7769, 0x701EE2E6,
					0x701D9166, 0x70709FCF, 0x6FF600E1, 0x709A98C8, 0x700053CE
				};
				unsigned uInitialServerKey = ~SendAccountBegin.AccountBegin.ServerKey;
				unsigned uGameKey = uGameKeyTable[uInitialServerKey % 10];
				CCriticalSection::Owner locker( m_csCM );
				m_theClientManager[dwIndex]->uServerKey = uGameKey * 214013U + 2531011U;
				m_theClientManager[dwIndex]->uClientKey = 0x1E278E7AU - uGameKey * 0x560397F7U;
			}

            WriteTransport( pSocket, reinterpret_cast< const BYTE * >( &SendAccountBegin ), sizeof( SendAccountBegin ) );
			TraceRequest( pSocket, _T("handshake"), reinterpret_cast< const BYTE * >( &SendAccountBegin ),
				sizeof( SendAccountBegin ), 0, CIPHER_PROTOCOL_TYPE, _T("account_begin") );

			/*
			 Output( GetTimeStamp() + _T("Add a client [ID:") + 
			 ToString( dwIndex ) + _T( "] - Current total is " ) +
			 ToString( m_usedClientNode.size() ) );
			 */
			/*
			 * Post a read command to IOCP
			 * Enter into a process that it can get data from network
			 */
			
			if ( m_pfnCallBackServerEvent )
			{
				m_pfnCallBackServerEvent( m_lpCallBackParam, dwIndex, enumClientConnectCreate );
			}

			Trace( pData->GetConnectionDetails().c_str(), GetTimeStamp() + _T(" Add a client [ID:") + 
				ToString( dwIndex ) + _T( "] - Current total is " ) +
				ToString( m_usedClientNode.size() ) );

			pSocket->Read();			
		}
		else
		{
			pSocket->AbortiveClose();
		}

		SAFE_RELEASE( pSocket );

		return true;
	}
	else
	{
		SAFE_DELETE( pData );
	}

	return false;
}

bool CIOCPServer::_HelperDelClient()
{
	CPerConnectionData *pData = ( CPerConnectionData * )( m_theDelClientQueue.Pop() );

	if ( NULL == pData )
	{
		return false;
	}
	
	Socket *pSocket = ( Socket * )( pData->GetConnectionSecondParam() );
	
	if ( pSocket )
	{
		pSocket->SetUserData( NULL );
	}
	
	DWORD dwIndex = pData->GetConnectionFirstParam();
	
	if ( ( DWORD )( -1 ) != dwIndex )
	{
		if ( m_pfnCallBackServerEvent )
		{
			m_pfnCallBackServerEvent( m_lpCallBackParam, dwIndex, enumClientConnectClose );
		}

		LPCLIENT_NODE pCN = NULL;
		{
			CCriticalSection::Owner	locker( m_csCM );
			
			pCN = ( LPCLIENT_NODE )( m_theClientManager[dwIndex] );
			
			if ( pCN )
			{
				pCN->pSocket = NULL;
				delete pCN->pTls;
				pCN->pTls = NULL;
				
				pCN->pReadBuffer->Empty();
				pCN->pRecvBuffer->Empty();
				pCN->pWriteBuffer->Empty();
			}
			
			/*
			* Erase ID from the used table
			*/
			
			m_usedClientNode.remove( dwIndex );
		}
		
		/*
		* A ID of call back is entered into the free table
		*/
		{
			CCriticalSection::Owner locker( m_csFCN );
			
			//m_freeClientNode.push( dwIndex );
			m_freeClientNode.push( dwIndex );
		}

		Trace( pData->GetConnectionDetails().c_str(), GetTimeStamp() + _T(" Del a client [ID:") + 
			ToString( dwIndex ) + _T( "] - Current total is " ) +
			ToString( m_usedClientNode.size() ) );
		
		/*
		Output( GetTimeStamp() + _T("Del a client [ID:") + 
		ToString( dwIndex ) + _T( "] - Current total is " ) + 
		ToString( m_usedClientNode.size() ) );
		*/		
	}
	
	SAFE_RELEASE( pSocket );
	SAFE_DELETE( pData );

	return true;
}

STDMETHODIMP CIOCPServer::RegisterMsgFilter( const unsigned long ulnClientID, 
		IMessageProcess* pfnMsgNotify )
{
	HRESULT hr = E_FAIL;

	if ( ulnClientID < m_nPlayerMaxCount )
	{
		LPCLIENT_NODE pCN = NULL;

		CCriticalSection::Owner	locker( m_csCM );

		pCN = ( LPCLIENT_NODE )( m_theClientManager[ulnClientID] );

		if ( pCN && pCN->pSocket )
		{
			pCN->pSocket->SetMessageCallback(pfnMsgNotify);
			
			hr = S_OK;
		}
	}

	return hr;
}

void CIOCPServer::ProcessMessage( Socket *pSocket, CIOBuffer *pBuffer )
{
	const size_t used = pBuffer->GetUsed();
	CPerConnectionData *pPCD = reinterpret_cast< CPerConnectionData * >( pSocket->GetUserPtr() );
	if ( !pPCD )
	{
		pSocket->Shutdown();
		return;
	}

	DWORD dwIndex = pPCD->GetConnectionFirstParam();
	{
		CCriticalSection::Owner locker( m_csCM );
		LPCLIENT_NODE pCN = ( LPCLIENT_NODE )( m_theClientManager[dwIndex] );
		if ( !pCN || pCN->pSocket != pSocket )
		{
			pSocket->Shutdown();
			return;
		}

		_ASSERT( pCN->uKeyMode == 0 );
		size_t processedSize = 0;
		while ( used - processedSize >= sizeof( WORD ) )
		{
			const BYTE *pData = pBuffer->GetBuffer() + processedSize;
			WORD messageSize = 0;
			memcpy( &messageSize, pData, sizeof( messageSize ) );
			if ( messageSize < GetMinimumMessageSize() || messageSize > pBuffer->GetSize() )
			{
				TraceRequest( pSocket, _T("reject"), pData, used - processedSize,
					0, -1, _T("invalid_frame_length") );
				pBuffer->Empty();
				pSocket->Shutdown();
				return;
			}
			if ( messageSize > used - processedSize )
			{
				break;
			}

			LONG sequence = pPCD->NextRequestSequence();
			TraceRequest( pSocket, _T("frame"), pData, messageSize, sequence, -1, _T("wire_complete") );
			KSG_DecodeBuf( messageSize - sizeof( WORD ),
				const_cast< unsigned char * >( pData + sizeof( WORD ) ), &( pCN->uClientKey ) );
			TraceRequest( pSocket, _T("request"), pData + sizeof( WORD ), messageSize - sizeof( WORD ),
				sequence, pData[sizeof( WORD )], _T("decoded_callback") );

			IMessageProcess *pCallback = pSocket->GetMessageCallback();
			if ( !pCallback )
			{
				TraceRequest( pSocket, _T("reject"), NULL, 0, sequence, pData[sizeof( WORD )],
					_T("callback_removed") );
				pBuffer->Empty();
				pSocket->Shutdown();
				return;
			}
			HRESULT hr = pCallback->OnMessage( ( VOID * )( pData + sizeof( WORD ) ),
				messageSize - sizeof( WORD ) );
			if ( FAILED( hr ) )
			{
				TraceRequest( pSocket, _T("reject"), NULL, 0, sequence, pData[sizeof( WORD )],
					_T("callback_failed") );
			}
			processedSize += messageSize;
		}

		if ( processedSize == used )
		{
			pBuffer->Empty();
		}
		else if ( processedSize )
		{
			pBuffer->RemoveBuffer( processedSize );
		}
	}

}

STDMETHODIMP CIOCPServer::EnableClientTls()
{
	CCriticalSection::Owner locker( m_csCM );
	if ( !m_usedClientNode.empty() )
	{
		return E_UNEXPECTED;
	}
	if ( !m_pTlsContext )
	{
		m_pTlsContext = new CClientTlsContext;
	}
	if ( !m_pTlsContext->Initialize() )
	{
		Output( _T("Client TLS: ") + m_pTlsContext->GetError() );
		return E_FAIL;
	}
	return S_OK;
}

bool CIOCPServer::WriteWire( Socket *pSocket, const BYTE *pData, size_t length )
{
	// TLS records can exceed an IOCP buffer. Queue ordered socket-sized pieces.
	while ( length )
	{
		size_t part = length < m_nNetworkBufferMaxLen ? length : m_nNetworkBufferMaxLen;
		if ( !pSocket->Write( pData, part, true ) )
		{
			pSocket->Shutdown();
			return false;
		}
		pData += part;
		length -= part;
	}
	return true;
}

bool CIOCPServer::WriteTransport( Socket *pSocket, const BYTE *pData, size_t length )
{
	CCriticalSection::Owner locker( m_csCM );
	CPerConnectionData *pConnection = reinterpret_cast< CPerConnectionData * >( pSocket->GetUserPtr() );
	if ( !pConnection || pConnection->GetConnectionFirstParam() >= m_nPlayerMaxCount )
	{
		return false;
	}
	LPCLIENT_NODE pCN = m_theClientManager[pConnection->GetConnectionFirstParam()];
	if ( !pCN || pCN->pSocket != pSocket )
	{
		return false;
	}
	if ( !pCN->pTls )
	{
		return WriteWire( pSocket, pData, length );
	}
	std::vector< BYTE > wire;
	if ( !pCN->pTls->Send( pData, length, wire ) )
	{
		TraceRequest( pSocket, _T("tls_error"), NULL, 0, 0, -1, pCN->pTls->GetError().c_str() );
		pSocket->Shutdown();
		return false;
	}
	return wire.empty() || WriteWire( pSocket, &wire[0], wire.size() );
}

void CIOCPServer::CheckTlsTimeouts()
{
	CCriticalSection::Owner locker( m_csCM );
	for ( LIST::iterator it = m_usedClientNode.begin(); it != m_usedClientNode.end(); ++it )
	{
		LPCLIENT_NODE pCN = m_theClientManager[*it];
		if ( pCN->pSocket && pCN->pTls && pCN->pTls->HasExpired() )
		{
			TraceRequest( pCN->pSocket, _T("tls_error"), NULL, 0, 0, -1, _T("handshake_timeout") );
			pCN->pSocket->Shutdown();
		}
	}
}
