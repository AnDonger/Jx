#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/pem.h>
#include <openssl/x509v3.h>

typedef std::vector< unsigned char > BYTES;

// These addresses belong only to the SHA256-checked 2026-10-30 client.
// Their signatures and calling conventions were checked in background GUI IDA.
static const DWORD SSL_CTX_NEW_EX = 0x008D9B60;
static const DWORD SSL_CTX_FREE = 0x008D92F0;
static const DWORD D2I_X509 = 0x00977960;
static const DWORD X509_STORE_ADD_CERT = 0x00974B50;
static const DWORD X509_FREE = 0x009777F0;
static const DWORD TLS_CONNECT = 0x0069E730;
static const DWORD LOGIN_TLS_CONNECT_CALL = 0x00679AF8;
static const DWORD GETPEERNAME_IAT = 0x0228C0A0;
static const char OFFICIAL_PUBLIC_KEY_PIN[] =
	"10914f18bfc58b69a7533b1a6834cd5757c610a73b111427a196239cb29e22aa";

static void Require( bool condition, const char *pMessage )
{
	if ( !condition )
	{
		throw std::runtime_error( pMessage );
	}
}

static BYTES ReadFileBytes( const char *pPath )
{
	std::ifstream file( pPath, std::ios::binary | std::ios::ate );
	Require( file.good(), "Cannot open input file." );
	std::streamoff length = file.tellg();
	Require( length > 0 && length < 256 * 1024 * 1024, "Invalid input file size." );
	BYTES data( static_cast< size_t >( length ) );
	file.seekg( 0 );
	file.read( reinterpret_cast< char * >( &data[0] ), length );
	Require( file.good(), "Cannot read complete input file." );
	return data;
}

static std::string GetSha256( const BYTES &data )
{
	unsigned char digest[EVP_MAX_MD_SIZE] = { 0 };
	unsigned int length = 0;
	Require( EVP_Digest( &data[0], data.size(), digest, &length, EVP_sha256(), NULL ) == 1,
		"SHA256 calculation failed." );
	std::string result;
	for ( unsigned int i = 0; i < length; ++i )
	{
		char byte[3] = { 0 };
		sprintf_s( byte, "%02X", digest[i] );
		result += byte;
	}
	return result;
}

static DWORD Align( DWORD value, DWORD alignment )
{
	Require( alignment && ( alignment & ( alignment - 1 ) ) == 0, "Invalid PE alignment." );
	return ( value + alignment - 1 ) & ~( alignment - 1 );
}

static DWORD GetFileOffset( const BYTES &image, DWORD address )
{
	const IMAGE_DOS_HEADER *pDos = reinterpret_cast< const IMAGE_DOS_HEADER * >( &image[0] );
	const IMAGE_NT_HEADERS32 *pNt = reinterpret_cast< const IMAGE_NT_HEADERS32 * >( &image[pDos->e_lfanew] );
	const IMAGE_SECTION_HEADER *pSection = IMAGE_FIRST_SECTION( pNt );
	DWORD rva = address - pNt->OptionalHeader.ImageBase;
	for ( WORD i = 0; i < pNt->FileHeader.NumberOfSections; ++i )
	{
		if ( rva >= pSection[i].VirtualAddress &&
			rva - pSection[i].VirtualAddress < pSection[i].SizeOfRawData )
		{
			return pSection[i].PointerToRawData + rva - pSection[i].VirtualAddress;
		}
	}
	throw std::runtime_error( "An analyzed address is outside file-backed sections." );
}

class CNativeCode
{
public:

	explicit CNativeCode( DWORD address )
		: m_address( address )
	{
	}

	void Bytes( std::initializer_list< unsigned char > bytes )
	{
		m_data.insert( m_data.end(), bytes.begin(), bytes.end() );
	}

	void Data( const BYTES &bytes )
	{
		m_data.insert( m_data.end(), bytes.begin(), bytes.end() );
	}

	void Dword( DWORD value )
	{
		for ( int i = 0; i < 4; ++i )
		{
			m_data.push_back( static_cast< unsigned char >( value >> ( 8 * i ) ) );
		}
	}

	void Label( const char *pName )
	{
		m_labels[pName] = m_data.size();
	}

	void Reference( const char *pName, bool relative )
	{
		REFERENCE entry = { pName, m_data.size(), relative };
		m_references.push_back( entry );
		Dword( 0 );
	}

	void Call( DWORD address )
	{
		Bytes( { 0xE8 } );
		Dword( address - ( m_address + static_cast< DWORD >( m_data.size() ) + 4 ) );
	}

	void Jump( const char *pLabel )
	{
		Bytes( { 0xE9 } );
		Reference( pLabel, true );
	}

	void JumpTo( DWORD address )
	{
		Bytes( { 0xE9 } );
		Dword( address - ( m_address + static_cast< DWORD >( m_data.size() ) + 4 ) );
	}

	void JumpIfZero( const char *pLabel )
	{
		Bytes( { 0x0F, 0x84 } );
		Reference( pLabel, true );
	}

	void AlignCode()
	{
		while ( m_data.size() % 16 )
		{
			Bytes( { 0xCC } );
		}
	}

	BYTES Finish()
	{
		for ( size_t i = 0; i < m_references.size(); ++i )
		{
			const REFERENCE &entry = m_references[i];
			Require( m_labels.count( entry.name ) != 0, "Undefined native-code label." );
			DWORD value = m_address + static_cast< DWORD >( m_labels[entry.name] );
			if ( entry.relative )
			{
				value -= m_address + static_cast< DWORD >( entry.offset ) + 4;
			}
			memcpy( &m_data[entry.offset], &value, sizeof( value ) );
		}
		return m_data;
	}

private:

	struct REFERENCE
	{
		std::string name;
		size_t offset;
		bool relative;
	};

	DWORD m_address;
	BYTES m_data;
	std::map< std::string, size_t > m_labels;
	std::vector< REFERENCE > m_references;
};

static BYTES MakeTrustHook( DWORD address, const BYTES &certificate )
{
	CNativeCode code( address );

	// SSL_CTX_new_ex(libctx, propertyQuery, method), cdecl.
	// Call the original initializer, then add the local PUBLIC certificate.
	code.Bytes( { 0x55 } );                         // push ebp
	code.Bytes( { 0x8B, 0xEC } );                   // mov ebp, esp
	code.Bytes( { 0x53 } );                         // push ebx
	code.Bytes( { 0x56 } );                         // push esi
	code.Bytes( { 0x83, 0xEC, 0x04 } );             // sub esp, 4
	code.Bytes( { 0xFF, 0x75, 0x10 } );             // push [ebp+16]
	code.Bytes( { 0xFF, 0x75, 0x0C } );             // push [ebp+12]
	code.Bytes( { 0xFF, 0x75, 0x08 } );             // push [ebp+8]
	code.Bytes( { 0xE8 } );
	code.Reference( "original", true );
	code.Bytes( { 0x83, 0xC4, 0x0C } );             // add esp, 12
	code.Bytes( { 0x8B, 0xD8 } );                   // mov ebx, eax (SSL_CTX)
	code.Bytes( { 0x85, 0xDB } );                   // test ebx, ebx
	code.JumpIfZero( "done" );

	code.Bytes( { 0xC7, 0x45, 0xF4 } );            // mov [ebp-12], certificate
	code.Reference( "certificate", false );
	code.Bytes( { 0x8D, 0x45, 0xF4 } );             // lea eax, [ebp-12]
	code.Bytes( { 0x68 } );
	code.Dword( static_cast< DWORD >( certificate.size() ) );
	code.Bytes( { 0x50 } );                         // push eax (&DER pointer)
	code.Bytes( { 0x6A, 0x00 } );                   // push NULL (new X509)
	code.Call( D2I_X509 );
	code.Bytes( { 0x83, 0xC4, 0x0C } );             // add esp, 12
	code.Bytes( { 0x8B, 0xF0 } );                   // mov esi, eax
	code.Bytes( { 0x85, 0xF6 } );                   // test esi, esi
	code.JumpIfZero( "failed" );

	code.Bytes( { 0x56 } );                         // push esi (public X509)
	code.Bytes( { 0xFF, 0x73, 0x14 } );             // push [ebx+20] (cert_store)
	code.Call( X509_STORE_ADD_CERT );
	code.Bytes( { 0x83, 0xC4, 0x08 } );             // add esp, 8
	code.Bytes( { 0x89, 0x45, 0xF4 } );             // mov [ebp-12], eax
	code.Bytes( { 0x56 } );                         // push esi
	code.Call( X509_FREE );                         // store retains its own reference
	code.Bytes( { 0x83, 0xC4, 0x04 } );             // add esp, 4
	code.Bytes( { 0x83, 0x7D, 0xF4, 0x01 } );       // cmp [ebp-12], 1
	code.Bytes( { 0x0F, 0x85 } );                   // jne failed
	code.Reference( "failed", true );
	code.Jump( "done" );

	code.Label( "failed" );
	code.Bytes( { 0x53 } );                         // push ebx
	code.Call( SSL_CTX_FREE );
	code.Bytes( { 0x83, 0xC4, 0x04 } );             // add esp, 4
	code.Bytes( { 0x33, 0xDB } );                   // xor ebx, ebx (fail closed)

	code.Label( "done" );
	code.Bytes( { 0x8B, 0xC3 } );                   // mov eax, ebx
	code.Bytes( { 0x83, 0xC4, 0x04 } );             // add esp, 4
	code.Bytes( { 0x5E } );                         // pop esi
	code.Bytes( { 0x5B } );                         // pop ebx
	code.Bytes( { 0x5D } );                         // pop ebp
	code.Bytes( { 0xC3 } );                         // ret (caller owns arguments)

	code.AlignCode();
	code.Label( "original" );
	code.Bytes( { 0x53, 0x8B, 0x5C, 0x24, 0x10 } ); // displaced complete prologue
	code.JumpTo( SSL_CTX_NEW_EX + 5 );
	code.Label( "certificate" );
	code.Data( certificate );
	return code.Finish();
}

static BYTES MakePinHook( DWORD address, const std::string &localPin )
{
	CNativeCode code( address );

	// The original connector verifies the peer SPKI after SSL_connect succeeds.
	// Select a second expected pin only for this socket's actual loopback peer.
	// Keep the original hostname, chain verification and SPKI comparison code.
	code.Bytes( { 0x9C, 0x60 } );                   // pushfd; pushad
	code.Bytes( { 0x83, 0xEC, 0x14 } );             // sockaddr_in + length
	code.Bytes( { 0xC7, 0x44, 0x24, 0x10 } );
	code.Dword( 16 );                              // address length
	code.Bytes( { 0x8D, 0x44, 0x24, 0x10, 0x50 } ); // push &length
	code.Bytes( { 0x8D, 0x44, 0x24, 0x04, 0x50 } ); // push &address
	code.Bytes( { 0xFF, 0x74, 0x24, 0x44 } );       // push original socket
	code.Bytes( { 0xFF, 0x15 } );
	code.Dword( GETPEERNAME_IAT );                  // stdcall, restores 12 bytes
	code.Bytes( { 0x85, 0xC0 } );                   // test eax, eax
	code.Bytes( { 0x0F, 0x85 } );
	code.Reference( "done", true );
	code.Bytes( { 0x66, 0x83, 0x3C, 0x24, 0x02 } ); // AF_INET
	code.Bytes( { 0x0F, 0x85 } );
	code.Reference( "done", true );
	code.Bytes( { 0x81, 0x7C, 0x24, 0x04 } );
	code.Dword( 0x0100007F );                      // exactly 127.0.0.1
	code.Bytes( { 0x0F, 0x85 } );
	code.Reference( "done", true );

	code.Bytes( { 0x8B, 0x74, 0x24, 0x40 } );       // original hostname
	code.Bytes( { 0x85, 0xF6 } );
	code.JumpIfZero( "done" );
	code.Bytes( { 0xBF } );                        // mov edi, hostname
	code.Reference( "hostname", false );
	code.Bytes( { 0xB9 } );
	code.Dword( sizeof( "fs.seayoo.com" ) );
	code.Bytes( { 0xFC, 0xF3, 0xA6 } );             // cld; repe cmpsb
	code.Bytes( { 0x0F, 0x85 } );
	code.Reference( "done", true );

	code.Bytes( { 0x8B, 0x74, 0x24, 0x44 } );       // original expected pin
	code.Bytes( { 0x85, 0xF6 } );
	code.JumpIfZero( "done" );
	code.Bytes( { 0xBF } );
	code.Reference( "officialPin", false );
	code.Bytes( { 0xB9 } );
	code.Dword( sizeof( OFFICIAL_PUBLIC_KEY_PIN ) );
	code.Bytes( { 0xFC, 0xF3, 0xA6 } );
	code.Bytes( { 0x0F, 0x85 } );
	code.Reference( "done", true );
	code.Bytes( { 0xC7, 0x44, 0x24, 0x44 } );
	code.Reference( "localPin", false );

	code.Label( "done" );
	code.Bytes( { 0x83, 0xC4, 0x14, 0x61, 0x9D } ); // restore stack, registers, flags
	code.JumpTo( TLS_CONNECT );                   // preserve the protected entry byte for byte
	code.AlignCode();
	code.Label( "hostname" );
	const char hostname[] = "fs.seayoo.com";
	code.Data( BYTES( hostname, hostname + sizeof( hostname ) ) );
	code.Label( "officialPin" );
	code.Data( BYTES( OFFICIAL_PUBLIC_KEY_PIN,
		OFFICIAL_PUBLIC_KEY_PIN + sizeof( OFFICIAL_PUBLIC_KEY_PIN ) ) );
	code.Label( "localPin" );
	code.Data( BYTES( localPin.c_str(), localPin.c_str() + localPin.size() + 1 ) );
	return code.Finish();
}

int main( int argc, char *argv[] )
{
	if ( argc != 4 )
	{
		fprintf( stderr, "Usage: PatchClientTrust.exe original-client.exe public-certificate.pem candidate-client.exe\n" );
		return 2;
	}
	try
	{
		OPENSSL_init_crypto( OPENSSL_INIT_NO_LOAD_CONFIG, NULL );
		BYTES image = ReadFileBytes( argv[1] );
		std::string originalHash = GetSha256( image );
		Require( originalHash == "2F795666BC02B4948B6B7416855272FF703C543CE404523AD44E4727912975BF",
			"The input is not the analyzed, unmodified 2026-10-30 client." );
		Require( GetFileAttributesA( argv[3] ) == INVALID_FILE_ATTRIBUTES,
			"The output already exists. Use a new candidate path." );

		BIO *pInput = BIO_new_file( argv[2], "rb" );
		Require( pInput != NULL, "Cannot open public certificate." );
		X509 *pCertificate = PEM_read_bio_X509( pInput, NULL, NULL, NULL );
		BIO_free( pInput );
		Require( pCertificate != NULL, "Cannot decode public certificate." );
		bool valid = X509_check_ca( pCertificate ) > 0 &&
			X509_cmp_current_time( X509_get0_notBefore( pCertificate ) ) < 0 &&
			X509_cmp_current_time( X509_get0_notAfter( pCertificate ) ) > 0 &&
			X509_check_host( pCertificate, "fs.seayoo.com", 0, 0, NULL ) == 1 &&
			X509_check_ip_asc( pCertificate, "127.0.0.1", 0 ) == 1;
		int derLength = i2d_X509( pCertificate, NULL );
		Require( valid && derLength > 0 && derLength < 16384, "Invalid local public certificate." );
		BYTES certificate( derLength );
		unsigned char *pDer = &certificate[0];
		i2d_X509( pCertificate, &pDer );
		int publicKeyLength = i2d_X509_PUBKEY( X509_get_X509_PUBKEY( pCertificate ), NULL );
		Require( publicKeyLength > 0 && publicKeyLength < 16384, "Invalid public-key encoding." );
		BYTES publicKey( publicKeyLength );
		unsigned char *pPublicKey = &publicKey[0];
		i2d_X509_PUBKEY( X509_get_X509_PUBKEY( pCertificate ), &pPublicKey );
		std::string localPin = GetSha256( publicKey );
		std::transform( localPin.begin(), localPin.end(), localPin.begin(),
			[]( unsigned char ch )
			{
				return static_cast< char >( ch >= 'A' && ch <= 'F' ? ch + 'a' - 'A' : ch );
			} );
		X509_free( pCertificate );

		IMAGE_DOS_HEADER *pDos = reinterpret_cast< IMAGE_DOS_HEADER * >( &image[0] );
		DWORD ntOffset = pDos->e_lfanew;
		IMAGE_NT_HEADERS32 *pNt = reinterpret_cast< IMAGE_NT_HEADERS32 * >( &image[ntOffset] );
		Require( pNt->Signature == IMAGE_NT_SIGNATURE &&
			pNt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
			pNt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
			pNt->OptionalHeader.ImageBase == 0x400000 &&
			( pNt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE ) == 0,
			"Unexpected PE architecture or relocation model." );
		DWORD sectionAlignment = pNt->OptionalHeader.SectionAlignment;
		DWORD fileAlignment = pNt->OptionalHeader.FileAlignment;
		WORD sectionCount = pNt->FileHeader.NumberOfSections;
		IMAGE_SECTION_HEADER *pSections = IMAGE_FIRST_SECTION( pNt );
		DWORD sectionTable = static_cast< DWORD >( reinterpret_cast< unsigned char * >( pSections ) - &image[0] );
		DWORD imageEnd = 0;
		DWORD firstRaw = MAXDWORD;
		for ( WORD i = 0; i < sectionCount; ++i )
		{
			imageEnd = ( std::max )( imageEnd, pSections[i].VirtualAddress +
				( std::max )( pSections[i].Misc.VirtualSize, pSections[i].SizeOfRawData ) );
			if ( pSections[i].SizeOfRawData )
			{
				firstRaw = ( std::min )( firstRaw, pSections[i].PointerToRawData );
			}
		}
		DWORD newHeader = sectionTable + sectionCount * sizeof( IMAGE_SECTION_HEADER );
		DWORD headerSize = Align( newHeader + sizeof( IMAGE_SECTION_HEADER ), fileAlignment );
		Require( headerSize <= firstRaw,
			"There is no unused PE section-header slot." );
		for ( size_t i = newHeader; i < newHeader + sizeof( IMAGE_SECTION_HEADER ); ++i )
		{
			Require( image[i] == 0, "The section-header slot is in use." );
		}
		DWORD hookOffset = GetFileOffset( image, SSL_CTX_NEW_EX );
		const unsigned char prologue[] = { 0x53, 0x8B, 0x5C, 0x24, 0x10 };
		Require( memcmp( &image[hookOffset], prologue, sizeof( prologue ) ) == 0,
			"The original SSL_CTX_new_ex prologue differs." );
		DWORD pinOffset = GetFileOffset( image, LOGIN_TLS_CONNECT_CALL );
		const unsigned char pinPrologue[] = { 0xE8, 0x33, 0x4C, 0x02, 0x00 };
		Require( memcmp( &image[pinOffset], pinPrologue, sizeof( pinPrologue ) ) == 0,
			"The original login TLS connector call differs." );
		DWORD sectionRva = Align( imageEnd, sectionAlignment );
		DWORD sectionVa = 0x400000 + sectionRva;
		DWORD rawOffset = Align( static_cast< DWORD >( image.size() ), fileAlignment );
		BYTES hook = MakeTrustHook( sectionVa, certificate );
		DWORD pinHookOffset = Align( static_cast< DWORD >( hook.size() ), 16 );
		hook.resize( pinHookOffset, 0xCC );
		BYTES pinHook = MakePinHook( sectionVa + pinHookOffset, localPin );
		hook.insert( hook.end(), pinHook.begin(), pinHook.end() );
		DWORD rawSize = Align( static_cast< DWORD >( hook.size() ), fileAlignment );
		image.resize( rawOffset + rawSize, 0 );
		std::copy( hook.begin(), hook.end(), image.begin() + rawOffset );

		pNt = reinterpret_cast< IMAGE_NT_HEADERS32 * >( &image[ntOffset] );
		IMAGE_SECTION_HEADER *pNew = reinterpret_cast< IMAGE_SECTION_HEADER * >( &image[newHeader] );
		memcpy( pNew->Name, ".jxtls", 6 );
		pNew->Misc.VirtualSize = static_cast< DWORD >( hook.size() );
		pNew->VirtualAddress = sectionRva;
		pNew->SizeOfRawData = rawSize;
		pNew->PointerToRawData = rawOffset;
		pNew->Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
		++pNt->FileHeader.NumberOfSections;
		pNt->OptionalHeader.SizeOfHeaders = ( std::max )( pNt->OptionalHeader.SizeOfHeaders, headerSize );
		pNt->OptionalHeader.SizeOfImage = Align( sectionRva + static_cast< DWORD >( hook.size() ), sectionAlignment );
		pNt->OptionalHeader.SizeOfCode += rawSize;
		pNt->OptionalHeader.CheckSum = 0;
		image[hookOffset] = 0xE9;
		DWORD displacement = sectionVa - ( SSL_CTX_NEW_EX + 5 );
		memcpy( &image[hookOffset + 1], &displacement, sizeof( displacement ) );
		image[pinOffset] = 0xE8;
		displacement = sectionVa + pinHookOffset - ( LOGIN_TLS_CONNECT_CALL + 5 );
		memcpy( &image[pinOffset + 1], &displacement, sizeof( displacement ) );

		std::ofstream output( argv[3], std::ios::binary );
		Require( output.good(), "Cannot create candidate file." );
		output.write( reinterpret_cast< const char * >( &image[0] ), image.size() );
		output.close();
		Require( output.good(), "Cannot write complete candidate file." );
		printf( "Original SHA256: %s\n", originalHash.c_str() );
		printf( "Candidate SHA256: %s\n", GetSha256( image ).c_str() );
		printf( "Public certificate DER SHA256: %s\n", GetSha256( certificate ).c_str() );
		printf( "Local public-key pin SHA256: %s\n", localPin.c_str() );
		printf( "Loopback pin hook VA: 0x%08lX. Original pin verification was preserved.\n",
			sectionVa + pinHookOffset );
		printf( "Trust hook VA: 0x%08lX. TLS verification functions were preserved.\n", sectionVa );
		return 0;
	}
	catch ( const std::exception &error )
	{
		fprintf( stderr, "%s\n", error.what() );
		return 1;
	}
}
