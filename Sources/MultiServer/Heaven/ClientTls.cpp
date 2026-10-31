#include "ClientTls.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

static std::string GetTlsError( const char *pOperation )
{
	char message[256] = { 0 };
	unsigned long error = ERR_get_error();
	ERR_error_string_n( error, message, sizeof( message ) );
	return std::string( pOperation ) + ": " + message;
}

CClientTlsContext::CClientTlsContext()
	: m_pContext( NULL )
{
}

CClientTlsContext::~CClientTlsContext()
{
	SSL_CTX_free( m_pContext );
}

bool CClientTlsContext::Initialize()
{
	if ( m_pContext )
	{
		return true;
	}

	ERR_clear_error();
	if ( OPENSSL_init_ssl( OPENSSL_INIT_NO_LOAD_CONFIG, NULL ) != 1 )
	{
		m_error = GetTlsError( "TLS library initialization" );
		return false;
	}
	char modulePath[MAX_PATH] = { 0 };
	DWORD moduleLength = GetModuleFileNameA( NULL, modulePath, MAX_PATH );
	char *pFileName = strrchr( modulePath, '\\' );
	if ( moduleLength == 0 || moduleLength >= MAX_PATH || !pFileName )
	{
		m_error = "TLS listener executable path is unavailable";
		return false;
	}
	*pFileName = 0;
	std::string resourcePath = std::string( modulePath ) +
		"\\..\\..\\Resources\\ServerConfig\\ClientTls\\";
	std::string certificatePath = resourcePath + "server-cert.pem";
	std::string keyPath = resourcePath + "server-key.pem";

	// Both public listeners use the approved, persistent local identity.
	// A missing identity must fail explicitly instead of changing its key.
	SSL_CTX *pContext = SSL_CTX_new( TLS_server_method() );
	bool ok = pContext &&
		SSL_CTX_set_min_proto_version( pContext, TLS1_3_VERSION ) == 1 &&
		SSL_CTX_set_max_proto_version( pContext, TLS1_3_VERSION ) == 1 &&
		SSL_CTX_use_certificate_file( pContext, certificatePath.c_str(), SSL_FILETYPE_PEM ) == 1 &&
		SSL_CTX_use_PrivateKey_file( pContext, keyPath.c_str(), SSL_FILETYPE_PEM ) == 1 &&
		SSL_CTX_check_private_key( pContext ) == 1;
	if ( ok )
	{
		X509 *pCertificate = SSL_CTX_get0_certificate( pContext );
		ok = pCertificate &&
			X509_cmp_current_time( X509_get0_notBefore( pCertificate ) ) < 0 &&
			X509_cmp_current_time( X509_get0_notAfter( pCertificate ) ) > 0;
		if ( !ok )
		{
			m_error = "TLS local certificate is not currently valid";
		}
	}
	if ( !ok )
	{
		if ( m_error.empty() )
		{
			m_error = GetTlsError( "TLS local certificate/private key loading" );
		}
		SSL_CTX_free( pContext );
		return false;
	}

	SSL_CTX_set_verify( pContext, SSL_VERIFY_NONE, NULL );
	SSL_CTX_set_num_tickets( pContext, 0 );
	SSL_CTX_set_session_cache_mode( pContext, SSL_SESS_CACHE_OFF );
	m_pContext = pContext;
	return true;
}

ssl_ctx_st *CClientTlsContext::GetContext() const
{
	return m_pContext;
}

const std::string &CClientTlsContext::GetError() const
{
	return m_error;
}

CClientTlsConnection::CClientTlsConnection( CClientTlsContext &context )
	: m_pSession( SSL_new( context.GetContext() ) ), m_started( GetTickCount() ),
	  m_pendingOffset( 0 ), m_pendingBytes( 0 )
{
	BIO *pInput = BIO_new( BIO_s_mem() );
	BIO *pOutput = BIO_new( BIO_s_mem() );
	if ( !m_pSession || !pInput || !pOutput )
	{
		BIO_free( pInput );
		BIO_free( pOutput );
		SSL_free( m_pSession );
		m_pSession = NULL;
		Fail( "TLS connection allocation" );
		return;
	}
	SSL_set_bio( m_pSession, pInput, pOutput );
	SSL_set_accept_state( m_pSession );
	SSL_set_mode( m_pSession, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER );
}

CClientTlsConnection::~CClientTlsConnection()
{
	SSL_free( m_pSession );
}

bool CClientTlsConnection::Fail( const char *pOperation )
{
	m_error = GetTlsError( pOperation );
	return false;
}

void CClientTlsConnection::DrainOutput( std::vector< BYTE > &wire )
{
	BYTE buffer[4096];
	int length = 0;
	while ( ( length = BIO_read( SSL_get_wbio( m_pSession ), buffer, sizeof( buffer ) ) ) > 0 )
	{
		wire.insert( wire.end(), buffer, buffer + length );
	}
}

bool CClientTlsConnection::FlushPending( std::vector< BYTE > &wire )
{
	while ( !m_pending.empty() && IsReady() )
	{
		// Keep AccountBegin and login initialization in separate TLS records,
		// matching the captured server's initial application sends.
		const std::vector< BYTE > &pending = m_pending.front();
		size_t written = 0;
		size_t length = pending.size() - m_pendingOffset;
		if ( length > 16384 )
		{
			length = 16384;
		}
		ERR_clear_error();
		int result = SSL_write_ex( m_pSession, &pending[m_pendingOffset], length, &written );
		int error = result == 1 ? SSL_ERROR_NONE : SSL_get_error( m_pSession, result );
		DrainOutput( wire );
		if ( result != 1 )
		{
			if ( error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE )
			{
				return true;
			}
			return Fail( "TLS write" );
		}
		m_pendingBytes -= written;
		m_pendingOffset += written;
		if ( m_pendingOffset == pending.size() )
		{
			m_pending.pop_front();
			m_pendingOffset = 0;
		}
	}
	return true;
}

bool CClientTlsConnection::Send( const BYTE *pData, size_t length, std::vector< BYTE > &wire )
{
	if ( !m_pSession || !m_error.empty() || length > 1024 * 1024 - m_pendingBytes )
	{
		return Fail( "TLS send queue" );
	}
	if ( length )
	{
		m_pending.push_back( std::vector< BYTE >( pData, pData + length ) );
		m_pendingBytes += length;
	}
	return FlushPending( wire );
}

bool CClientTlsConnection::Receive( const BYTE *pData, size_t length,
	std::vector< BYTE > &plain, std::vector< BYTE > &wire )
{
	if ( !m_pSession || !m_error.empty() ||
		length + BIO_ctrl_pending( SSL_get_rbio( m_pSession ) ) > 128 * 1024 )
	{
		return Fail( "TLS receive queue" );
	}
	ERR_clear_error();
	if ( BIO_write( SSL_get_rbio( m_pSession ), pData, static_cast< int >( length ) ) != static_cast< int >( length ) )
	{
		return Fail( "TLS input" );
	}
	if ( !IsReady() )
	{
		int result = SSL_do_handshake( m_pSession );
		int error = result == 1 ? SSL_ERROR_NONE : SSL_get_error( m_pSession, result );
		DrainOutput( wire );
		if ( result != 1 )
		{
			if ( error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE )
			{
				return true;
			}
			return Fail( "TLS handshake" );
		}
	}

	if ( !FlushPending( wire ) )
	{
		return false;
	}
	for ( ;; )
	{
		BYTE buffer[16384];
		size_t received = 0;
		ERR_clear_error();
		int result = SSL_read_ex( m_pSession, buffer, sizeof( buffer ), &received );
		int error = result == 1 ? SSL_ERROR_NONE : SSL_get_error( m_pSession, result );
		DrainOutput( wire );
		if ( result == 1 )
		{
			plain.insert( plain.end(), buffer, buffer + received );
			continue;
		}
		if ( error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE )
		{
			return FlushPending( wire );
		}
		if ( error == SSL_ERROR_ZERO_RETURN )
		{
			SSL_shutdown( m_pSession );
			DrainOutput( wire );
			m_error = "TLS peer closed";
			return false;
		}
		return Fail( "TLS read" );
	}
}

bool CClientTlsConnection::IsReady() const
{
	return m_pSession && SSL_is_init_finished( m_pSession ) == 1;
}

bool CClientTlsConnection::HasExpired() const
{
	return !IsReady() && GetTickCount() - m_started > 15000;
}

std::string CClientTlsConnection::GetDescription() const
{
	if ( !IsReady() )
	{
		return "TLS pending";
	}
	return std::string( SSL_get_version( m_pSession ) ) + " " + SSL_get_cipher_name( m_pSession );
}

const std::string &CClientTlsConnection::GetError() const
{
	return m_error;
}
