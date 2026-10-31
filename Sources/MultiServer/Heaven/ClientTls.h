#ifndef __INCLUDE_CLIENTTLS_H__
#define __INCLUDE_CLIENTTLS_H__

#include <windows.h>
#include <deque>
#include <string>
#include <vector>

struct ssl_ctx_st;
struct ssl_st;

/*
 * TLS belongs to the existing public client listeners. Internal services keep
 * their original transport. Callers serialize each connection with m_csCM.
 */
class CClientTlsContext
{
public:

	CClientTlsContext();
	~CClientTlsContext();

	bool Initialize();
	ssl_ctx_st *GetContext() const;
	const std::string &GetError() const;

private:

	ssl_ctx_st *m_pContext;
	std::string m_error;

	CClientTlsContext( const CClientTlsContext & );
	CClientTlsContext &operator=( const CClientTlsContext & );
};

class CClientTlsConnection
{
public:

	explicit CClientTlsConnection( CClientTlsContext &context );
	~CClientTlsConnection();

	bool Receive( const BYTE *pData, size_t length,
		std::vector< BYTE > &plain, std::vector< BYTE > &wire );
	bool Send( const BYTE *pData, size_t length, std::vector< BYTE > &wire );

	bool IsReady() const;
	bool HasExpired() const;
	std::string GetDescription() const;
	const std::string &GetError() const;

private:

	bool FlushPending( std::vector< BYTE > &wire );
	void DrainOutput( std::vector< BYTE > &wire );
	bool Fail( const char *pOperation );

	ssl_st *m_pSession;
	DWORD m_started;
	std::deque< std::vector< BYTE > > m_pending;
	size_t m_pendingOffset;
	size_t m_pendingBytes;
	std::string m_error;

	CClientTlsConnection( const CClientTlsConnection & );
	CClientTlsConnection &operator=( const CClientTlsConnection & );
};

#endif
