#include "stdafx.h"
#include "IDBRoleServer.h"
#include "DBTable.h"
#include "DBBackup.h"
#include "../../../../Headers/KGmProtocol.h"
#include "CRC32.h"
#include "IniFile.h"
#include "Utils.h"
#include "CriticalSection.h"
#include <sql.h>
#include <sqlext.h>

#pragma comment( lib, "odbc32.lib" )

#include <iostream>
#include <strstream>

using namespace std;

#include "Macro.h"

static ZDBTable *db_table = NULL;
static size_t nMaxRoleCount_InAccount = 3;
static CDBBackup::TStatData GameStatData;//游戏统计数据（服务器初始化时填入）
static CDBBackup* DBBackup = NULL;

extern HWND hListOutput;

static SQLHENV g_hRoleEnvironment = SQL_NULL_HENV;
static SQLHDBC g_hRoleConnection = SQL_NULL_HDBC;
static OnlineGameLib::Win32::CCriticalSection g_csRoleDatabase;

static void LogRoleDatabaseError( SQLSMALLINT nType, SQLHANDLE hHandle )
{
	SQLCHAR state[6];
	SQLCHAR message[512];
	SQLINTEGER native = 0;
	SQLSMALLINT length = 0;
	if ( SQL_SUCCEEDED( SQLGetDiagRec( nType, hHandle, 1, state, &native,
		message, sizeof( message ), &length ) ) )
	{
		char text[640];
		sprintf( text, "SQL role database [%s] %ld: %s", state, native, message );
		AddOutputString( hListOutput, text );
		OnlineGameLib::Win32::TraceNetworkData( text, NULL, 0 );
	}
}

class CRoleStatement
{
public:
	SQLHSTMT m_hStatement;

	CRoleStatement()
		: m_hStatement( SQL_NULL_HSTMT )
	{
		SQLAllocHandle( SQL_HANDLE_STMT, g_hRoleConnection, &m_hStatement );
	}

	~CRoleStatement()
	{
		if ( m_hStatement != SQL_NULL_HSTMT )
		{
			SQLFreeHandle( SQL_HANDLE_STMT, m_hStatement );
		}
	}

	bool Execute( const char *szSql )
	{
		if ( m_hStatement != SQL_NULL_HSTMT && SQL_SUCCEEDED(
			SQLExecDirectA( m_hStatement, ( SQLCHAR * )szSql, SQL_NTS ) ) )
		{
			return true;
		}
		LogRoleDatabaseError( SQL_HANDLE_STMT, m_hStatement );
		return false;
	}

	bool Name( int nParameter, WCHAR *szValue )
	{
		return SQL_SUCCEEDED( SQLBindParameter( m_hStatement, nParameter, SQL_PARAM_INPUT,
			SQL_C_WCHAR, SQL_WVARCHAR, 32, 0, szValue, 0, NULL ) );
	}
};

static bool RoleNameToWide( const char *szName, WCHAR szWide[32] )
{
	if ( !szName || !memchr( szName, 0, 32 ) || !szName[0] )
	{
		return false;
	}
	return MultiByteToWideChar( 936, MB_ERR_INVALID_CHARS, szName, -1, szWide, 32 ) > 0;
}

static bool CheckRoleData( const TRoleData *pData, size_t nLength )
{
	if ( !pData || nLength < sizeof( TRoleData ) - 1 || nLength >= 64 * 1024 - 32 ||
		pData->dwDataLen != nLength || !memchr( pData->BaseInfo.szName, 0, 32 ) ||
		!memchr( pData->BaseInfo.caccname, 0, 32 ) || !pData->BaseInfo.szName[0] ||
		!pData->BaseInfo.caccname[0] )
	{
		return false;
	}
	int offsets[] = { pData->dwFSkillOffset, pData->dwLSkillOffset,
		pData->dwTaskOffset, pData->dwItemOffset, pData->dwFriendOffset };
	int counts[] = { pData->nFightSkillCount, pData->nLiveSkillCount,
		pData->nTaskCount, pData->nItemCount, pData->nFriendCount };
	size_t sizes[] = { sizeof( TDBSkillData ), sizeof( TDBSkillData ),
		sizeof( TDBTaskData ), sizeof( TDBItemData ), 0 };
	for ( int i = 0; i < 5; ++i )
	{
		if ( counts[i] < 0 || offsets[i] < sizeof( TRoleData ) - 1 ||
			static_cast< size_t >( offsets[i] ) > nLength ||
			static_cast< size_t >( counts[i] ) * sizes[i] > nLength - offsets[i] )
		{
			return false;
		}
	}
	return true;
}

static bool OpenRoleDatabase()
{
	OnlineGameLib::Win32::CIniFile file;
	std::string path = OnlineGameLib::Win32::GetAppFullPath( NULL ) + "DataBase.ini";
	file.SetFile( path.c_str() );
	char server[256];
	char database[128];
	char user[128];
	char password[128];
	file.ReadString( "role", "Server", server, sizeof( server ), "" );
	file.ReadString( "role", "DataBase", database, sizeof( database ), "" );
	file.ReadString( "role", "User", user, sizeof( user ), "" );
	file.ReadString( "role", "PassWord", password, sizeof( password ), "" );
	if ( !server[0] || !database[0] )
	{
		return false;
	}
	if ( !SQL_SUCCEEDED( SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE, &g_hRoleEnvironment ) ) ||
		!SQL_SUCCEEDED( SQLSetEnvAttr( g_hRoleEnvironment, SQL_ATTR_ODBC_VERSION,
			( SQLPOINTER )SQL_OV_ODBC3, 0 ) ) ||
		!SQL_SUCCEEDED( SQLAllocHandle( SQL_HANDLE_DBC, g_hRoleEnvironment, &g_hRoleConnection ) ) )
	{
		return false;
	}
	SQLSetConnectAttr( g_hRoleConnection, SQL_LOGIN_TIMEOUT, ( SQLPOINTER )10, 0 );
	char connection[1024];
	sprintf( connection, "DRIVER={SQL Server};SERVER=%s;DATABASE=%s;", server, database );
	if ( user[0] )
	{
		strcat( connection, "UID=" );
		strcat( connection, user );
		strcat( connection, ";PWD=" );
		strcat( connection, password );
		strcat( connection, ";" );
	}
	else
	{
		strcat( connection, "Trusted_Connection=Yes;" );
	}
	SQLRETURN result = SQLDriverConnectA( g_hRoleConnection, NULL, ( SQLCHAR * )connection,
		SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT );
	SecureZeroMemory( password, sizeof( password ) );
	SecureZeroMemory( connection, sizeof( connection ) );
	if ( !SQL_SUCCEEDED( result ) )
	{
		LogRoleDatabaseError( SQL_HANDLE_DBC, g_hRoleConnection );
		return false;
	}
	CRoleStatement statement;
	return statement.Execute(
		"SET NOCOUNT ON; "
		"IF OBJECT_ID(N'dbo.Role_Info',N'U') IS NULL BEGIN "
		"CREATE TABLE dbo.Role_Info("
		"iid int IDENTITY(1,1) NOT NULL,"
		"cusercode nvarchar(32) COLLATE Chinese_PRC_BIN2 NOT NULL PRIMARY KEY,"
		"caccname nvarchar(32) COLLATE Chinese_PRC_CI_AS NOT NULL,"
		"RoleData varbinary(max) NOT NULL,btPortrait tinyint NOT NULL DEFAULT(0)); "
		"CREATE INDEX IX_Role_Info_Account ON dbo.Role_Info(caccname,iid); END; "
		"IF OBJECT_ID(N'dbo.RoleStorageMigration',N'U') IS NULL "
		"CREATE TABLE dbo.RoleStorageMigration(MigrationID int NOT NULL PRIMARY KEY); "
		"IF OBJECT_ID(N'dbo.RoleLegacyArchive',N'U') IS NULL "
		"CREATE TABLE dbo.RoleLegacyArchive(OriginalKey varbinary(128) NOT NULL PRIMARY KEY,"
		"RoleData varbinary(max) NOT NULL,Imported bit NOT NULL); "
		"SELECT TOP 0 cusercode,caccname,RoleData,btPortrait FROM dbo.Role_Info" );
}


static HANDLE hDeadLockThread = NULL;		//检查数据库有没有死锁的线程
static HANDLE hRemoveLogThread = NULL;	//删除没用日志文件的线程

HWND hListOutput = NULL;	//输出list

int get_account(DB *db, const DBT *pkey, const DBT *pdata, DBT *ikey) 
{
	//给定一个完整的buffer，得到account作为索引
	memset( ikey, 0, sizeof( DBT ) );
	TRoleData *pRoleData = (TRoleData *)pdata->data;

	ikey->data = pRoleData->BaseInfo.caccname;
	ikey->size = strlen( pRoleData->BaseInfo.caccname ) + 1;

	return 0;
}

//==========检查数据库有没有死锁的线程 Add By Fellow 2003.9.10==============
DWORD WINAPI DeadlockProc(LPVOID lpParameter) {
	while(!db_table->bStop) {
		Sleep(5 * 1000);	//5秒钟一次
		db_table->deadlock();
	}
	return 0;
}
//==========删除没用日志文件的线程 Add By Fellow 2003.9.10==============
DWORD WINAPI RemoveLogProc(LPVOID lpParameter) {
	while(!db_table->bStop) {
		db_table->removeLog();
		Sleep(60 * 60 * 1000);			//1小时一次
	}
	return 0;
}

BOOL InitDBInterface( size_t nMaxRoleCount )
{
	OnlineGameLib::Win32::TraceNetworkData( "stage=role_database_initialize", NULL, 0 );
	nMaxRoleCount_InAccount = nMaxRoleCount;
	if ( !OpenRoleDatabase() )
	{
		ReleaseDBInterface();
		return FALSE;
	}

	OnlineGameLib::Win32::TraceNetworkData( "stage=role_database_sql_connected", NULL, 0 );
	CRoleStatement migration;
	if ( !migration.Execute( "SELECT COUNT(*) FROM dbo.RoleStorageMigration WHERE MigrationID=1" ) ||
		SQLFetch( migration.m_hStatement ) != SQL_SUCCESS )
	{
		return FALSE;
	}
	int migrated = 0;
	SQLGetData( migration.m_hStatement, 1, SQL_C_LONG, &migrated, sizeof( migrated ), NULL );
	SQLCloseCursor( migration.m_hStatement );
	if ( migrated )
	{
		return TRUE;
	}

	// Import existing file-library roles once; SQL Server is authoritative afterwards.
	if ( GetFileAttributes( "database\\roledb" ) != INVALID_FILE_ATTRIBUTES )
	{
	OnlineGameLib::Win32::TraceNetworkData( "stage=role_database_legacy_open", NULL, 0 );
		ZDBTable legacy( "database", "roledb" );
		legacy.addIndex( get_account );
		if ( !legacy.open() )
		{
			return FALSE;
		}
	OnlineGameLib::Win32::TraceNetworkData( "stage=role_database_legacy_read", NULL, 0 );
		ZCursor *cursor = legacy.first();
		OnlineGameLib::Win32::TraceNetworkData( "stage=role_database_legacy_first", NULL, 0 );
		while ( cursor )
		{
			TRoleData *role = ( TRoleData * )cursor->data;
			bool valid = CheckRoleData( role, cursor->size );
			if ( valid )
			{
				WCHAR name[32];
				valid = RoleNameToWide( role->BaseInfo.szName, name );
				for ( int i = 0; valid && name[i]; ++i )
				{
					if ( name[i] < 32 || name[i] == 127 )
					{
						valid = false;
					}
				}
			}
			int result = valid ? SaveRoleInfo( cursor->data, NULL, TRUE, 0 ) : 0;
			if ( result == -2 )
			{
				char existing[64 * 1024];
				int length = 0;
				GetRoleInfo( existing, role->BaseInfo.szName, length );
				if ( !length || stricmp( ( ( TRoleData * )existing )->BaseInfo.caccname,
					role->BaseInfo.caccname ) )
				{
					result = 0;
				}
			}
			int imported = result == 1 || result == -2;
			CRoleStatement archive;
			SQLLEN keyLength = cursor->key_size;
			SQLLEN dataLength = cursor->size;
			bool archived = SQL_SUCCEEDED( SQLBindParameter( archive.m_hStatement, 1,
				SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, keyLength, 0,
				cursor->key, keyLength, &keyLength ) ) &&
				SQL_SUCCEEDED( SQLBindParameter( archive.m_hStatement, 2,
				SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, dataLength, 0,
				cursor->data, dataLength, &dataLength ) ) &&
				SQL_SUCCEEDED( SQLBindParameter( archive.m_hStatement, 3,
				SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0,
				&imported, 0, NULL ) ) && archive.Execute(
				"DECLARE @key varbinary(128)=?,@data varbinary(max)=?,@imported int=?; "
				"IF NOT EXISTS(SELECT 1 FROM dbo.RoleLegacyArchive WHERE OriginalKey=@key) "
				"INSERT dbo.RoleLegacyArchive(OriginalKey,RoleData,Imported) VALUES(@key,@data,@imported);" );
			if ( !archived || ( valid && !imported ) )
			{
				legacy.closeCursor( cursor );
				legacy.close();
				return FALSE;
			}
			OnlineGameLib::Win32::TraceNetworkData( "stage=role_database_legacy_import imported=" +
				OnlineGameLib::Win32::ToString( imported ) + " length=" +
				OnlineGameLib::Win32::ToString( cursor->size ), NULL, 0 );
			if ( !legacy.next( cursor ) )
			{
				cursor = NULL;
			}
		}
		legacy.close();
	}
	return migration.Execute( "INSERT dbo.RoleStorageMigration(MigrationID) VALUES(1)" ) ? TRUE : FALSE;
}

void ReleaseDBInterface()
{
	OnlineGameLib::Win32::CCriticalSection::Owner lock( g_csRoleDatabase );
	if ( g_hRoleConnection != SQL_NULL_HDBC )
	{
		SQLDisconnect( g_hRoleConnection );
		SQLFreeHandle( SQL_HANDLE_DBC, g_hRoleConnection );
		g_hRoleConnection = SQL_NULL_HDBC;
	}
	if ( g_hRoleEnvironment != SQL_NULL_HENV )
	{
		SQLFreeHandle( SQL_HANDLE_ENV, g_hRoleEnvironment );
		g_hRoleEnvironment = SQL_NULL_HENV;
	}
}


//[wxb 2003-7-23]
void SetRoleInfoForGM(int nInfoID, char* pRoleBuffer, char* strUser, int nBufLen)
{
	char* aBuffer = new char[64 * 1024];
	TRoleData* pRoleData = NULL;
	int size;

	GetRoleInfo(aBuffer, strUser, size);
	if(size)
	{
		pRoleData = (TRoleData*)aBuffer;
		switch (nInfoID)
		{
		case gm_role_entergame_position:
			ASSERT(nBufLen == sizeof(GM_ROLE_DATA_SUB_ENTER_POS));
			memcpy(&(pRoleData->BaseInfo.ientergameid), pRoleBuffer, sizeof(GM_ROLE_DATA_SUB_ENTER_POS));
			break;
		default:
			ASSERT(0);
			return;
			break;
		}
		DWORD crc = CRC32( 0, aBuffer, size - sizeof( DWORD ) );
		memcpy( aBuffer + size - sizeof( DWORD ), &crc, sizeof( crc ) );
		SaveRoleInfo(aBuffer, strUser, false);
	}
	
    delete [] aBuffer;
}

//[wxb 2003-7-22]
void *GetRoleInfoForGM( int nInfoID, char *pRoleBuffer, char *strUser, int &nBufLen )
{
	char data[64 * 1024];
	int length = 0;
	GetRoleInfo( data, strUser, length );
	nBufLen = 0;
	if ( length && nInfoID == gm_role_entergame_position )
	{
		TRoleData *role = reinterpret_cast< TRoleData * >( data );
		nBufLen = 3 * sizeof( int );
		memcpy( pRoleBuffer, &role->BaseInfo.ientergameid, nBufLen );
	}
	return pRoleBuffer;
}

void *GetRoleInfo( char *pRoleBuffer, char *strUser, int &nBufLen, BYTE *pPortrait )
{
	nBufLen = 0;
	if ( pPortrait )
	{
		*pPortrait = 0;
	}
	WCHAR name[32];
	if ( !pRoleBuffer || !RoleNameToWide( strUser, name ) )
	{
		return pRoleBuffer;
	}
	OnlineGameLib::Win32::CCriticalSection::Owner lock( g_csRoleDatabase );
	CRoleStatement statement;
	if ( statement.Name( 1, name ) &&
		statement.Execute( "SELECT RoleData,btPortrait FROM dbo.Role_Info WHERE cusercode=?" ) &&
		SQLFetch( statement.m_hStatement ) == SQL_SUCCESS )
	{
		SQLLEN length = 0;
		if ( SQLGetData( statement.m_hStatement, 1, SQL_C_BINARY, pRoleBuffer,
			64 * 1024 - 32, &length ) == SQL_SUCCESS && length > 0 &&
			CheckRoleData( ( TRoleData * )pRoleBuffer, length ) )
		{
			nBufLen = length;
			if ( pPortrait )
			{
				BYTE portrait = 0;
				SQLLEN portraitLength = 0;
				if ( SQLGetData( statement.m_hStatement, 2, SQL_C_UTINYINT, &portrait,
					sizeof( portrait ), &portraitLength ) == SQL_SUCCESS )
				{
					*pPortrait = portrait;
				}
			}
		}
	}
	return pRoleBuffer;
}

int SaveRoleInfo( char *pRoleBuffer, const char *strUser, BOOL bCreate, int nPortrait )
{
	TRoleData *role = reinterpret_cast< TRoleData * >( pRoleBuffer );
	if ( !role || !CheckRoleData( role, role->dwDataLen ) )
	{
		return 0;
	}
	WCHAR name[32];
	WCHAR account[32];
	if ( !RoleNameToWide( role->BaseInfo.szName, name ) ||
		!RoleNameToWide( role->BaseInfo.caccname, account ) ||
		( strUser && strUser[0] && strcmp( strUser, role->BaseInfo.szName ) ) )
	{
		return 0;
	}
	if ( !bCreate )
	{
		DWORD crc = 0;
		memcpy( &crc, pRoleBuffer + role->dwDataLen - sizeof( crc ), sizeof( crc ) );
		if ( CRC32( 0, role, role->dwDataLen - sizeof( crc ) ) != crc )
		{
			return 0;
		}
	}
	OnlineGameLib::Win32::CCriticalSection::Owner lock( g_csRoleDatabase );
	CRoleStatement statement;
	SQLLEN dataLength = role->dwDataLen;
	int maxCount = nMaxRoleCount_InAccount;
	if ( !statement.Name( 1, name ) || !statement.Name( 2, account ) ||
		!SQL_SUCCEEDED( SQLBindParameter( statement.m_hStatement, 3, SQL_PARAM_INPUT,
			SQL_C_BINARY, SQL_LONGVARBINARY, dataLength, 0, pRoleBuffer, dataLength, &dataLength ) ) ||
		!SQL_SUCCEEDED( SQLBindParameter( statement.m_hStatement, 4, SQL_PARAM_INPUT,
			SQL_C_LONG, SQL_INTEGER, 0, 0, &nPortrait, 0, NULL ) ) ||
		!SQL_SUCCEEDED( SQLBindParameter( statement.m_hStatement, 5, SQL_PARAM_INPUT,
			SQL_C_LONG, SQL_INTEGER, 0, 0, &maxCount, 0, NULL ) ) )
	{
		return 0;
	}
	const char *sql = bCreate ?
		"SET NOCOUNT ON; SET XACT_ABORT ON; "
		"DECLARE @name nvarchar(32)=?,@account nvarchar(32)=?,@data varbinary(max)=?,"
		"@portrait int=?,@maximum int=?,@result int=0; "
		"SET TRANSACTION ISOLATION LEVEL SERIALIZABLE; BEGIN TRANSACTION; "
		"IF EXISTS(SELECT 1 FROM dbo.Role_Info WITH(UPDLOCK,HOLDLOCK) WHERE cusercode=@name) SET @result=-2; "
		"ELSE IF (SELECT COUNT(*) FROM dbo.Role_Info WITH(UPDLOCK,HOLDLOCK) WHERE caccname=@account)>=@maximum SET @result=-3; "
		"ELSE BEGIN INSERT dbo.Role_Info(cusercode,caccname,RoleData,btPortrait) "
		"VALUES(@name,@account,@data,CASE WHEN @portrait<0 THEN 0 ELSE @portrait END); SET @result=1; END; "
		"COMMIT TRANSACTION; SET TRANSACTION ISOLATION LEVEL READ COMMITTED; SELECT @result;" :
		"SET NOCOUNT ON; DECLARE @name nvarchar(32)=?,@account nvarchar(32)=?,@data varbinary(max)=?,"
		"@portrait int=?,@maximum int=?; "
		"UPDATE dbo.Role_Info SET RoleData=@data,btPortrait=CASE WHEN @portrait<0 THEN btPortrait ELSE @portrait END "
		"WHERE cusercode=@name AND caccname=@account; SELECT @@ROWCOUNT;";
	if ( !statement.Execute( sql ) || SQLFetch( statement.m_hStatement ) != SQL_SUCCESS )
	{
		SQLEndTran( SQL_HANDLE_DBC, g_hRoleConnection, SQL_ROLLBACK );
		return 0;
	}
	int result = 0;
	SQLLEN length = 0;
	SQLGetData( statement.m_hStatement, 1, SQL_C_LONG, &result, sizeof( result ), &length );
	return result;
}

int GetRoleListCurrentOfAccount( const char *szAccountName,
	S3DBI_RoleBaseInfoCurrent *pRoleList, int nMaxCount )
{
	WCHAR account[32];
	if ( !pRoleList || nMaxCount < 1 || nMaxCount > 3 || !RoleNameToWide( szAccountName, account ) )
	{
		return -1;
	}
	OnlineGameLib::Win32::CCriticalSection::Owner lock( g_csRoleDatabase );
	CRoleStatement statement;
	if ( !statement.Name( 1, account ) || !statement.Execute(
		"SELECT RoleData,btPortrait FROM dbo.Role_Info WHERE caccname=? ORDER BY iid" ) )
	{
		return -1;
	}
	int count = 0;
	SQLRETURN result = SQL_SUCCESS;
	while ( count < nMaxCount && ( result = SQLFetch( statement.m_hStatement ) ) == SQL_SUCCESS )
	{
		char data[64 * 1024];
		SQLLEN length = 0;
		SQLCHAR portrait = 0;
		if ( SQLGetData( statement.m_hStatement, 1, SQL_C_BINARY, data, sizeof( data ), &length ) != SQL_SUCCESS ||
			length <= 0 || !CheckRoleData( ( TRoleData * )data, length ) ||
			!SQL_SUCCEEDED( SQLGetData( statement.m_hStatement, 2, SQL_C_UTINYINT,
				&portrait, sizeof( portrait ), NULL ) ) )
		{
			return -1;
		}
		TRoleBaseInfo &base = ( ( TRoleData * )data )->BaseInfo;
		S3DBI_RoleBaseInfoCurrent &role = pRoleList[count++];
		ZeroMemory( &role, sizeof( role ) );
		memcpy( role.szName, base.szName, sizeof( role.szName ) );
		role.Sex = base.bSex;
		role.Series = base.ifiveprop;
		role.Level = base.ifightlevel;
		role.btPortrait = portrait;
		role.dwExperience = base.fightexp;
		role.dwLastMapID = base.cUseRevive ? base.irevivalid : base.ientergameid;
	}
	return result == SQL_ERROR ? -1 : count;
}

int GetRoleListOfAccount( char *szAccountName, S3DBI_RoleBaseInfo *pRoleList, int nMaxCount )
{
	S3DBI_RoleBaseInfoCurrent roles[3];
	int count = GetRoleListCurrentOfAccount( szAccountName, roles, nMaxCount );
	for ( int i = 0; i < count; ++i )
	{
		ZeroMemory( &pRoleList[i], sizeof( pRoleList[i] ) );
		memcpy( pRoleList[i].szName, roles[i].szName, 32 );
		pRoleList[i].Sex = roles[i].Sex;
		pRoleList[i].Series = roles[i].Series;
		pRoleList[i].Level = roles[i].Level;
	}
	return count;
}

bool DeleteRole( const char *strUser, const char *szAccountName )
{
	WCHAR name[32];
	WCHAR account[32];
	if ( !RoleNameToWide( strUser, name ) ||
		( szAccountName && !RoleNameToWide( szAccountName, account ) ) )
	{
		return false;
	}
	OnlineGameLib::Win32::CCriticalSection::Owner lock( g_csRoleDatabase );
	CRoleStatement statement;
	if ( !statement.Name( 1, name ) || ( szAccountName && !statement.Name( 2, account ) ) )
	{
		return false;
	}
	const char *sql = szAccountName ?
		"SET NOCOUNT ON; DELETE dbo.Role_Info WHERE cusercode=? AND caccname=?; SELECT @@ROWCOUNT;" :
		"SET NOCOUNT ON; DELETE dbo.Role_Info WHERE cusercode=?; SELECT @@ROWCOUNT;";
	if ( !statement.Execute( sql ) || SQLFetch( statement.m_hStatement ) != SQL_SUCCESS )
	{
		return false;
	}
	int count = 0;
	SQLGetData( statement.m_hStatement, 1, SQL_C_LONG, &count, sizeof( count ), NULL );
	return count == 1;
}


//------------------------------------------------------------------------
//数据库备份与数据统计 Add By Fellow At 2003.08.14
bool StartBackupTimer(int aTime)
{
	// The legacy Berkeley backup is not a SQL Server database backup.
	if ( !db_table )
	{
		return false;
	}//开始运行备份线程
	DBBackup = new CDBBackup( "database", "roledb", db_table );
	DBBackup->SaveStatInfo();
	bool aStartResult = DBBackup->Open(aTime);
	return aStartResult;
}

bool StopBackupTimer()
{//结束运行备份线程
	if(!DBBackup)return false;
	while(IsBackupWorking()){}//等待备份线程结束

	bool aResult = DBBackup->Close();
	delete DBBackup;
	DBBackup = NULL;
	return aResult;
}

bool SuspendBackupTimer()
{//挂起线程
	if(!DBBackup)return false;
	return DBBackup->Suspend();
}

bool ResumeBackupTimer()
{//继续运行线程
	if(!DBBackup)return false;
	return DBBackup->Resume();
}

bool IsBackupThreadWorking()
{//线程是否正在运行
	if(!DBBackup)return false;
	return DBBackup->IsWorking();
}

bool IsBackupWorking()
{//是否在备份
	if(!DBBackup)return false;
	return DBBackup->IsBackuping();
}

bool DoManualBackup()
{//手工备份
	if(!DBBackup)return false;
	return DBBackup->ManualBackup();
}

bool GetGameStat(TGAME_STAT_DATA* aStatData)
{
	if(!DBBackup)return false;
	TGAME_STAT_DATA tmpStatData = DBBackup->GetSendStatData();
	memcpy(aStatData, &tmpStatData, sizeof(TGAME_STAT_DATA));
	return true;
}

void AddOutputString(HWND hListCtrl, char* aStr)
{//添加操作输出文字
	if ( hListCtrl && ::IsWindow( hListCtrl ) )
	{
		int nCount = ::SendMessage( hListCtrl, LB_GETCOUNT, 0, 0 );
		if(nCount >= 100)
		{
			::SendMessage( hListCtrl, LB_DELETESTRING, 100, 0 );
		}
		int nIndex = ::SendMessage( hListCtrl, LB_INSERTSTRING, 0, ( LPARAM )aStr );//把新的信息添到第一个
	}
}