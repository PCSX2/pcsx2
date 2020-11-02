#include "inifile.h"
#include <algorithm>
#include <iostream>
#include <fstream>

// Only valid if C++
#ifndef __cplusplus
#error C++ compiler required.
#endif

// In the event you want to trace the calls can define _TRACE_CINIFILE
#ifdef _TRACE_CINIFILE
#define _CINIFILE_DEBUG
#endif

// _CRLF is used in the Save() function
// The class will read the correct data regardless of how the file linefeeds are defined <CRLF> or <CR>
// It is best to use the linefeed that is default to the system. This reduces issues if needing to modify
// the file with ie. notepad.exe which doesn't recognize unix linefeeds.
#ifdef _WIN32 // Windows default is \r\n
#ifdef _FORCE_UNIX_LINEFEED
#define _CRLFA "\n"
#define _CRLFW L"\n"
#else
#define _CRLFA "\r\n"
#define _CRLFW L"\r\n"
#endif

#else // Standard format is \n for unix
#ifdef _FORCE_WINDOWS_LINEFEED
#define _CRLFA "\r\n"
#define _CRLFW L"\r\n"
#else
#define _CRLFA "\n"
#define _CRLFW L"\n"
#endif
#endif

// Convert wstring to string
std::string wstr_to_str(const std::wstring& arg)
{
    std::string res( arg.length(), '\0' );
    wcstombs( const_cast<char*>(res.data()) , arg.c_str(), arg.length());
    return res;
}

// Convert string to wstring
std::wstring str_to_wstr(const std::string& arg)
{
    std::wstring res(arg.length(), L'\0');
    mbstowcs(const_cast<wchar_t*>(res.data()), arg.c_str(), arg.length());
    return res;
}

// Helper Functions
void RTrim(std::string &str, const std::string& chars = " \t")
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "RTrim()" << std::endl;
#endif
    str.erase(str.find_last_not_of(chars)+1);
}

void LTrim(std::string &str, const std::string& chars = " \t" )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "LTrim()" << std::endl;
#endif
    str.erase(0, str.find_first_not_of(chars));
}

void Trim( std::string& str , const std::string& chars = " \t" )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "Trim()" << std::endl;
#endif
    str.erase(str.find_last_not_of(chars)+1);
    str.erase(0, str.find_first_not_of(chars));
}

// Stream Helpers

std::ostream& operator<<(std::ostream& output, CIniFileA& obj)
{
    obj.Save( output );
    return output;
}

std::istream& operator>>(std::istream& input, CIniFileA& obj)
{
    obj.Load( input );
    return input;
}

std::istream& operator>>(std::istream& input, CIniMergeA merger)
{
    return merger(input);
}

// CIniFileA class methods

CIniFileA::CIniFileA()
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::CIniFileA()" << std::endl;
#endif
}

CIniFileA::~CIniFileA()
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::~CIniFileA()" << std::endl;
#endif
    RemoveAllSections( );
}

const char* const CIniFileA::LF = _CRLFA;

void CIniFileA::Save( std::ostream& output )
{
    std::string sSection;

    for( SecIndexA::iterator itr = m_sections.begin() ; itr != m_sections.end() ; ++itr )
    {
        sSection = "[" + (*itr)->GetSectionName() + "]";

#ifdef _CINIFILE_DEBUG
        std::cout <<  "Writing Section " << sSection << std::endl;
#endif

        output << sSection << _CRLFA;

        for( KeyIndexA::iterator klitr = (*itr)->m_keys.begin() ; klitr !=  (*itr)->m_keys.end() ; ++klitr )
        {
            std::string sKey = (*klitr)->GetKeyName() + "=" + (*klitr)->GetValue();
#ifdef _CINIFILE_DEBUG
            std::cout <<  "Writing Key [" << sKey << "]" << std::endl;
#endif
            output << sKey << _CRLFA;
        }
    }
}

bool CIniFileA::Save( const std::string& fileName )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::Save() - " << fileName << std::endl;
#endif

    std::ofstream output;

    output.open( fileName.c_str() , std::ios::binary );

    if( !output.is_open() )
        return false;

    Save(output);

    output.close();
    return true;
}


void CIniFileA::Load( std::istream& input , bool bMerge )
{
    if( !bMerge )
        RemoveAllSections();

    CIniSectionA* pSection = NULL;
    std::string sRead;
    enum { KEY , SECTION , COMMENT , OTHER };

    while( std::getline( input , sRead ) )
    {

        // Trim all whitespace on the left
        LTrim( sRead );
        // Trim any returns
        RTrim( sRead , "\n\r");

        if( !sRead.empty() )
        {
            unsigned int nType = ( sRead.find_first_of("[") == 0 && ( sRead[sRead.find_last_not_of(" \t\r\n")] == ']' ) ) ? SECTION : OTHER ;
            nType = ( (nType == OTHER) && ( sRead.find_first_of("=") != std::string::npos && sRead.find_first_of("=") > 0 ) )? KEY : nType ;
            nType = ( (nType == OTHER) && ( sRead.find_first_of("#") == 0) ) ? COMMENT : nType ;

            switch( nType )
            {
            case SECTION:
#ifdef _CINIFILE_DEBUG
                std::cout <<  "Parsing: Secton - " << sRead << std::endl;
#endif
                pSection = AddSection( sRead.substr( 1 , sRead.size() - 2 ) );
                break;

            case KEY:
            {
#ifdef _CINIFILE_DEBUG
                std::cout <<  "Parsing: Key - " << sRead << std::endl;
#endif
                // Check to ensure valid section... or drop the keys listed
                if( pSection )
                {
                    size_t iFind = sRead.find_first_of("=");
                    std::string sKey = sRead.substr(0,iFind);
                    std::string sValue = sRead.substr(iFind + 1);
                    CIniKeyA* pKey = pSection->AddKey( sKey );
                    if( pKey )
                    {
#ifdef _CINIFILE_DEBUG
                        std::cout <<  "Parsing: Key Value - " << sValue << std::endl;
#endif
                        pKey->SetValue( sValue );
                    }
                }
            }
            break;
            case COMMENT:
#ifdef _CINIFILE_DEBUG
                std::cout <<  "Parsing: Comment - " << sRead << std::endl;
#endif
                break;
            case OTHER:
#ifdef _CINIFILE_DEBUG
                std::cout <<  "Parsing: Other - " << sRead << std::endl;
#endif
                break;
            }
        }
    }
}

bool CIniFileA::Load(const std::string& fileName , bool bMerge )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::Load() - " << fileName << std::endl;
#endif

    std::ifstream input;

    input.open( fileName.c_str() , std::ios::binary);

    if( !input.is_open() )
        return false;

    Load( input , bMerge );

    input.close();
    return true;
}

const SecIndexA& CIniFileA::GetSections() const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::GetSections()" << std::endl;
#endif
    return m_sections;
}


CIniSectionA* CIniFileA::GetSection( std::string sSection ) const
{
#ifdef _CINIFILE_DEBUG
    std::cout << "CIniFileA::GetSection()" << std::endl;
#endif
    Trim(sSection);
    SecIndexA::const_iterator itr = _find_sec( sSection );
    if( itr != m_sections.end() )
        return *itr;
    return NULL;
}

CIniSectionA* CIniFileA::AddSection( std::string sSection )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::AddSection()" << std::endl;
#endif

    Trim(sSection);
    SecIndexA::const_iterator itr = _find_sec( sSection );
    if( itr == m_sections.end() )
    {
        // Note constuctor doesn't trim the string so it is trimmed above
        CIniSectionA* pSection = new CIniSectionA( this , sSection );
        m_sections.insert(pSection);
        return pSection;
    }
    else
        return *itr;
}


std::string CIniFileA::GetKeyValue( const std::string& sSection, const std::string& sKey ) const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::GetKeyValue()" << std::endl;
#endif
    std::string sValue;
    CIniSectionA* pSec = GetSection( sSection );
    if( pSec )
    {
        CIniKeyA* pKey = pSec->GetKey( sKey );
        if( pKey )
            sValue = pKey->GetValue();
    }
    return sValue;
}

void CIniFileA::SetKeyValue( const std::string& sSection, const std::string& sKey, const std::string& sValue )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::SetKeyValue()" << std::endl;
#endif
    CIniSectionA* pSec = AddSection( sSection );
    if( pSec )
    {
        CIniKeyA* pKey = pSec->AddKey( sKey );
        if( pKey )
            pKey->SetValue( sValue );
    }
}


void CIniFileA::RemoveSection( std::string sSection )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::RemoveSection()" << std::endl;
#endif
    Trim(sSection);
    SecIndexA::iterator itr = _find_sec( sSection );
    if( itr != m_sections.end() )
    {
        delete *itr;
        m_sections.erase( itr );
    }
}

void CIniFileA::RemoveSection( CIniSectionA* pSection )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::RemoveSection()" << std::endl;
#endif
    // No trim since internal object not from user
    SecIndexA::iterator itr = _find_sec( pSection->m_sSectionName );
    if( itr != m_sections.end() )
    {
        delete *itr;
        m_sections.erase( itr );
    }
}

void CIniFileA::RemoveAllSections( )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::RemoveAllSections()" << std::endl;
#endif
    for( SecIndexA::iterator itr = m_sections.begin() ; itr != m_sections.end() ; ++itr )
    {
#ifdef _CINIFILE_DEBUG
        std::cout <<  "Deleting Section: CIniSectionAName[" << (*itr)->GetSectionName() << "]" << std::endl;
#endif
        delete *itr;
    }
    m_sections.clear();
}


bool CIniFileA::RenameSection( const std::string& sSectionName  , const std::string& sNewSectionName )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::RenameSection()" << std::endl;
#endif
    // Note string trims are done in lower calls.
    bool bRval = false;
    CIniSectionA* pSec = GetSection( sSectionName );
    if( pSec )
    {
        bRval = pSec->SetSectionName( sNewSectionName );
    }
    return bRval;
}

bool CIniFileA::RenameKey( const std::string& sSectionName  , const std::string& sKeyName , const std::string& sNewKeyName)
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniFileA::RenameKey()" << std::endl;
#endif
    // Note string trims are done in lower calls.
    bool bRval = false;
    CIniSectionA* pSec = GetSection( sSectionName );
    if( pSec != NULL)
    {
        CIniKeyA* pKey = pSec->GetKey( sKeyName );
        if( pKey != NULL )
            bRval = pKey->SetKeyName( sNewKeyName );
    }
    return bRval;
}

// Returns a constant iterator to a section by name, string is not trimmed
SecIndexA::const_iterator CIniFileA::_find_sec( const std::string& sSection ) const
{
    CIniSectionA bogus(NULL,sSection);
    return m_sections.find( &bogus );
}

// Returns an iterator to a section by name, string is not trimmed
SecIndexA::iterator CIniFileA::_find_sec( const std::string& sSection )
{
    CIniSectionA bogus(NULL,sSection);
    return m_sections.find( &bogus );
}

// CIniFileA functions end here

// CIniSectionA functions start here

CIniSectionA::CIniSectionA( CIniFileA* pIniFile , const std::string& sSectionName ) : m_pIniFile(pIniFile) , m_sSectionName(sSectionName)
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::CIniSectionA()" << std::endl;
#endif
}


CIniSectionA::~CIniSectionA()
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::~CIniSectionA()" << std::endl;
#endif
    RemoveAllKeys();
}

CIniKeyA* CIniSectionA::GetKey( std::string sKeyName ) const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::GetKey()" << std::endl;
#endif
    Trim(sKeyName);
    KeyIndexA::const_iterator itr = _find_key( sKeyName );
    if( itr != m_keys.end() )
        return *itr;
    return NULL;
}

void CIniSectionA::RemoveAllKeys()
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::RemoveAllKeys()" << std::endl;
#endif
    for( KeyIndexA::iterator itr = m_keys.begin() ; itr != m_keys.end() ; ++itr )
    {
#ifdef _CINIFILE_DEBUG
        std::cout <<  "Deleting Key: " << (*itr)->GetKeyName() << std::endl;
#endif
        delete *itr;
    }
    m_keys.clear();
}

void CIniSectionA::RemoveKey( std::string sKey )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::RemoveKey()" << std::endl;
#endif
    Trim( sKey );
    KeyIndexA::iterator itr = _find_key( sKey );
    if( itr != m_keys.end() )
    {
        delete *itr;
        m_keys.erase( itr );
    }
}

void CIniSectionA::RemoveKey( CIniKeyA* pKey )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::RemoveKey()" << std::endl;
#endif
    // No trim is done to improve efficiency since CIniKeyA* should already be trimmed
    KeyIndexA::iterator itr = _find_key( pKey->m_sKeyName );
    if( itr != m_keys.end() )
    {
        delete *itr;
        m_keys.erase( itr );
    }
}

CIniKeyA* CIniSectionA::AddKey( std::string sKeyName )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::AddKey()" << std::endl;
#endif
    Trim(sKeyName);
    KeyIndexA::const_iterator itr = _find_key( sKeyName );
    if( itr == m_keys.end() )
    {
        // Note constuctor doesn't trim the string so it is trimmed above
        CIniKeyA* pKey = new CIniKeyA( this , sKeyName );
        m_keys.insert(pKey);
        return pKey;
    }
    else
        return *itr;
}

bool CIniSectionA::SetSectionName( std::string sSectionName )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::SetSectionName()" << std::endl;
#endif
    Trim(sSectionName);
    // Does this already exist.
    if( m_pIniFile->_find_sec( sSectionName ) == m_pIniFile->m_sections.end() )
    {
#ifdef _CINIFILE_DEBUG
        std::cout <<  "Setting Section Name: [" << m_sSectionName <<  "] --> [" << sSectionName << "]" << std::endl;
#endif

        // Find the current section if one exists and remove it since we are renaming
        SecIndexA::iterator itr = m_pIniFile->_find_sec( m_sSectionName );

        // Just to be safe make sure the old section exists
        if( itr != m_pIniFile->m_sections.end() )
            m_pIniFile->m_sections.erase(itr);

        // Change name prior to ensure tree balance
        m_sSectionName = sSectionName;

        // Set the new map entry we know should not exist
        m_pIniFile->m_sections.insert(this);

        return true;
    }
    else
    {
#ifdef _CINIFILE_DEBUG
        std::cout <<  "Section existed could not rename" << std::endl;
#endif
        return false;
    }
}

std::string CIniSectionA::GetSectionName() const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::GetSectionName()" << std::endl;
#endif
    return m_sSectionName;
}

const KeyIndexA& CIniSectionA::GetKeys() const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::GetKeys()" << std::endl;
#endif
    return m_keys;
}

std::string CIniSectionA::GetKeyValue( std::string sKey ) const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::GetKeyValue()" << std::endl;
#endif
    std::string sValue;
    CIniKeyA* pKey = GetKey( sKey );
    if( pKey )
    {
        sValue = pKey->GetValue();
    }
    return sValue;
}

void CIniSectionA::SetKeyValue( std::string sKey, const std::string& sValue )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::SetKeyValue()" << std::endl;
#endif
    CIniKeyA* pKey = AddKey( sKey );
    if( pKey )
    {
        pKey->SetValue( sValue );
    }
}

// Returns a constant iterator to a key by name, string is not trimmed
KeyIndexA::const_iterator CIniSectionA::_find_key( const std::string& sKey ) const
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::_find_key() const" << std::endl;
#endif
    CIniKeyA bogus(NULL,sKey);
    return m_keys.find( &bogus );
}

// Returns an iterator to a key by name, string is not trimmed
KeyIndexA::iterator CIniSectionA::_find_key( const std::string& sKey )
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniSectionA::_find_key()" << std::endl;
#endif
    CIniKeyA bogus(NULL,sKey);
    return m_keys.find( &bogus );
}

// CIniSectionA function end here

// CIniKeyA Functions Start Here

CIniKeyA::CIniKeyA( CIniSectionA* pSection , const std::string& sKeyName ) : m_pSection(pSection) , m_sKeyName(sKeyName)
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniKeyA::CIniKeyA()" << std::endl;
#endif
}


CIniKeyA::~CIniKeyA()
{
#ifdef _CINIFILE_DEBUG
    std::cout <<  "CIniKeyA::~CIniKeyA()" << std::endl;
#endif
}

void CIniKeyA::SetValue( const std::string& sValue )
{
#ifdef _CINIFILE_DEBUG
    std::cout << "CIniKeyA::SetValue()" << std::endl;
#endif
    m_sValue = sValue;
}

std::string CIniKeyA::GetValue() const
{
#ifdef _CINIFILE_DEBUG
    std::cout << "CIniKeyA::GetValue()" << std::endl;
#endif
    return m_sValue;
}

bool CIniKeyA::SetKeyName( std::string sKeyName )
{
#ifdef _CINIFILE_DEBUG
    std::cout << "CIniKeyA::SetKeyName()" << std::endl;
#endif
    Trim( sKeyName );

    // Check for key name conflict
    if( m_pSection->_find_key( sKeyName ) == m_pSection->m_keys.end() )
    {
        KeyIndexA::iterator itr = m_pSection->_find_key( m_sKeyName );

        // Find the old map entry and remove it
        if( itr != m_pSection->m_keys.end() )
            m_pSection->m_keys.erase( itr );

        // Change name prior to ensure tree balance
        m_sKeyName = sKeyName;

        // Make the new map entry
        m_pSection->m_keys.insert(this);
        return true;
    }
    else
    {
#ifdef _CINIFILE_DEBUG
        std::cout << "Could not set key name, key by that name already exists!" << std::endl;
#endif
        return false;
    }
}

std::string CIniKeyA::GetKeyName() const
{
#ifdef _CINIFILE_DEBUG
    std::cout << "CIniKeyA::GetKeyName()" << std::endl;
#endif
    return m_sKeyName;
}

// End of CIniKeyA Functions


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//              WIDE FUNCTIONS HERE
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Helper Functions
void RTrim(std::wstring &str, const std::wstring& chars = L" \t")
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"RTrim()" << std::endl;
#endif
    str.erase(str.find_last_not_of(chars)+1);
}

void LTrim(std::wstring &str, const std::wstring& chars = L" \t" )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"LTrim()" << std::endl;
#endif
    str.erase(0, str.find_first_not_of(chars));
}

void Trim( std::wstring& str , const std::wstring& chars = L" \t" )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"Trim()" << std::endl;
#endif
    str.erase(str.find_last_not_of(chars)+1);
    str.erase(0, str.find_first_not_of(chars));
}

// Stream Helpers
std::wostream& operator<<(std::wostream& output, CIniFileW& obj)
{
    obj.Save( output );
    return output;
}

std::wistream& operator>>(std::wistream& input, CIniFileW& obj)
{
    obj.Load( input );
    return input;
}

std::wistream& operator>>(std::wistream& input, CIniMergeW merger)
{
    return merger(input);
}

CIniFileW::CIniFileW()
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::CIniFileW()" << std::endl;
#endif
}

CIniFileW::~CIniFileW()
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::~CIniFileW()" << std::endl;
#endif
    RemoveAllSections( );
}

const wchar_t* const CIniFileW::LF = _CRLFW;

void CIniFileW::Save( std::wostream& output )
{
    std::wstring sSection;

    for( SecIndexW::iterator itr = m_sections.begin() ; itr != m_sections.end() ; ++itr )
    {
        sSection = L"[" + (*itr)->GetSectionName() + L"]";

#ifdef _CINIFILE_DEBUG
        std::wcout <<  L"Writing Section " << sSection << std::endl;
#endif

        output << sSection << _CRLFA;

        for( KeyIndexW::iterator klitr = (*itr)->m_keys.begin() ; klitr !=  (*itr)->m_keys.end() ; ++klitr )
        {
            std::wstring sKey = (*klitr)->GetKeyName() + L"=" + (*klitr)->GetValue();
#ifdef _CINIFILE_DEBUG
            std::wcout <<  L"Writing Key [" << sKey << L"]" << std::endl;
#endif
            output << sKey << _CRLFA;
        }
    }
}

bool CIniFileW::Save( const std::wstring& fileName )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::Save() - " << fileName << std::endl;
#endif

    std::wofstream output;

#if defined(_MSC_VER) && (_MSC_VER >= 1300)
   output.open( fileName.c_str() , std::ios::binary );
#else
    output.open( wstr_to_str(fileName).c_str() , std::ios::binary );
#endif

    if( !output.is_open() )
        return false;

    Save(output);

    output.close();
    return true;
}


void CIniFileW::Load( std::wistream& input , bool bMerge )
{
    if( !bMerge )
        RemoveAllSections();

    CIniSectionW* pSection = NULL;
    std::wstring sRead;
    enum { KEY , SECTION , COMMENT , OTHER };

    while( std::getline( input , sRead ) )
    {

        // Trim all whitespace on the left
        LTrim( sRead );
        // Trim any returns
        RTrim( sRead , L"\n\r");

        if( !sRead.empty() )
        {
            unsigned int nType = ( sRead.find_first_of(L"[") == 0 && ( sRead[sRead.find_last_not_of(L" \t\r\n")] == L']' ) ) ? SECTION : OTHER ;
            nType = ( (nType == OTHER) && ( sRead.find_first_of(L"=") != std::wstring::npos && sRead.find_first_of(L"=") > 0 ) ) ? KEY : nType ;
            nType = ( (nType == OTHER) && ( sRead.find_first_of(L"#") == 0) ) ? COMMENT : nType ;

            switch( nType )
            {
            case SECTION:
#ifdef _CINIFILE_DEBUG
                std::wcout <<  L"Parsing: Secton - " << sRead << std::endl;
#endif
                pSection = AddSection( sRead.substr( 1 , sRead.size() - 2 ) );
                break;

            case KEY:
            {
#ifdef _CINIFILE_DEBUG
                std::wcout <<  L"Parsing: Key - " << sRead << std::endl;
#endif
                // Check to ensure valid section... or drop the keys listed
                if( pSection )
                {
                    size_t iFind = sRead.find_first_of(L"=");
                    std::wstring sKey = sRead.substr(0,iFind);
                    std::wstring sValue = sRead.substr(iFind + 1);
                    CIniKeyW* pKey = pSection->AddKey( sKey );
                    if( pKey )
                    {
#ifdef _CINIFILE_DEBUG
                        std::wcout <<  L"Parsing: Key Value - " << sValue << std::endl;
#endif
                        pKey->SetValue( sValue );
                    }
                }
            }
            break;
            case COMMENT:
#ifdef _CINIFILE_DEBUG
                std::wcout <<  L"Parsing: Comment - " << sRead << std::endl;
#endif
                break;
            case OTHER:
#ifdef _CINIFILE_DEBUG
                std::wcout <<  L"Parsing: Other - " << sRead << std::endl;
#endif
                break;
            }
        }
    }
}

bool CIniFileW::Load(const std::wstring& fileName , bool bMerge )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::Load() - " << fileName << std::endl;
#endif

    std::wifstream input;

#if defined(_MSC_VER) && (_MSC_VER >= 1300)
    input.open( fileName.c_str() , std::ios::binary );
#else
    input.open( wstr_to_str(fileName).c_str() , std::ios::binary );
#endif

    if( !input.is_open() )
        return false;

    Load( input , bMerge );

    input.close();
    return true;
}

const SecIndexW& CIniFileW::GetSections() const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::GetSections()" << std::endl;
#endif
    return m_sections;
}


CIniSectionW* CIniFileW::GetSection( std::wstring sSection ) const
{
#ifdef _CINIFILE_DEBUG
    std::wcout << L"CIniFileW::GetSection()" << std::endl;
#endif
    Trim(sSection);
    SecIndexW::const_iterator itr = _find_sec( sSection );
    if( itr != m_sections.end() )
        return *itr;
    return NULL;
}

CIniSectionW* CIniFileW::AddSection( std::wstring sSection )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::AddSection()" << std::endl;
#endif

    Trim(sSection);
    SecIndexW::const_iterator itr = _find_sec( sSection );
    if( itr == m_sections.end() )
    {
        // Note constuctor doesn't trim the string so it is trimmed above
        CIniSectionW* pSection = new CIniSectionW( this , sSection );
        m_sections.insert(pSection);
        return pSection;
    }
    else
        return *itr;
}


std::wstring CIniFileW::GetKeyValue( const std::wstring& sSection, const std::wstring& sKey ) const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::GetKeyValue()" << std::endl;
#endif
    std::wstring sValue;
    CIniSectionW* pSec = GetSection( sSection );
    if( pSec )
    {
        CIniKeyW* pKey = pSec->GetKey( sKey );
        if( pKey )
            sValue = pKey->GetValue();
    }
    return sValue;
}

void CIniFileW::SetKeyValue( const std::wstring& sSection, const std::wstring& sKey, const std::wstring& sValue )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::SetKeyValue()" << std::endl;
#endif
    CIniSectionW* pSec = AddSection( sSection );
    if( pSec )
    {
        CIniKeyW* pKey = pSec->AddKey( sKey );
        if( pKey )
            pKey->SetValue( sValue );
    }
}


void CIniFileW::RemoveSection( std::wstring sSection )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::RemoveSection()" << std::endl;
#endif
    Trim(sSection);
    SecIndexW::iterator itr = _find_sec( sSection );
    if( itr != m_sections.end() )
    {
        delete *itr;
        m_sections.erase( itr );
    }
}

void CIniFileW::RemoveSection( CIniSectionW* pSection )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::RemoveSection()" << std::endl;
#endif
    // No trim since internal object not from user
    SecIndexW::iterator itr = _find_sec( pSection->m_sSectionName );
    if( itr != m_sections.end() )
    {
        delete *itr;
        m_sections.erase( itr );
    }
}

void CIniFileW::RemoveAllSections( )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::RemoveAllSections()" << std::endl;
#endif
    for( SecIndexW::iterator itr = m_sections.begin() ; itr != m_sections.end() ; ++itr )
    {
#ifdef _CINIFILE_DEBUG
        std::wcout <<  L"Deleting Section: CIniSectionWName[" << (*itr)->GetSectionName() << L"]" << std::endl;
#endif
        delete *itr;
    }
    m_sections.clear();
}


bool CIniFileW::RenameSection( const std::wstring& sSectionName  , const std::wstring& sNewSectionName )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::RenameSection()" << std::endl;
#endif
    // Note string trims are done in lower calls.
    bool bRval = false;
    CIniSectionW* pSec = GetSection( sSectionName );
    if( pSec )
    {
        bRval = pSec->SetSectionName( sNewSectionName );
    }
    return bRval;
}

bool CIniFileW::RenameKey( const std::wstring& sSectionName  , const std::wstring& sKeyName , const std::wstring& sNewKeyName)
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniFileW::RenameKey()" << std::endl;
#endif
    // Note string trims are done in lower calls.
    bool bRval = false;
    CIniSectionW* pSec = GetSection( sSectionName );
    if( pSec != NULL)
    {
        CIniKeyW* pKey = pSec->GetKey( sKeyName );
        if( pKey != NULL )
            bRval = pKey->SetKeyName( sNewKeyName );
    }
    return bRval;
}

// Returns a constant iterator to a section by name, string is not trimmed
SecIndexW::const_iterator CIniFileW::_find_sec( const std::wstring& sSection ) const
{
    CIniSectionW bogus(NULL,sSection);
    return m_sections.find( &bogus );
}

// Returns an iterator to a section by name, string is not trimmed
SecIndexW::iterator CIniFileW::_find_sec( const std::wstring& sSection )
{
    CIniSectionW bogus(NULL,sSection);
    return m_sections.find( &bogus );
}

// CIniFileW functions end here

// CIniSectionW functions start here

CIniSectionW::CIniSectionW( CIniFileW* pIniFile , const std::wstring& sSectionName ) : m_pIniFile(pIniFile) , m_sSectionName(sSectionName)
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::CIniSectionW()" << std::endl;
#endif
}


CIniSectionW::~CIniSectionW()
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::~CIniSectionW()" << std::endl;
#endif
    RemoveAllKeys();
}

CIniKeyW* CIniSectionW::GetKey( std::wstring sKeyName ) const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::GetKey()" << std::endl;
#endif
    Trim(sKeyName);
    KeyIndexW::const_iterator itr = _find_key( sKeyName );
    if( itr != m_keys.end() )
        return *itr;
    return NULL;
}

void CIniSectionW::RemoveAllKeys()
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::RemoveAllKeys()" << std::endl;
#endif
    for( KeyIndexW::iterator itr = m_keys.begin() ; itr != m_keys.end() ; ++itr )
    {
#ifdef _CINIFILE_DEBUG
        std::wcout <<  L"Deleting Key: " << (*itr)->GetKeyName() << std::endl;
#endif
        delete *itr;
    }
    m_keys.clear();
}

void CIniSectionW::RemoveKey( std::wstring sKey )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::RemoveKey()" << std::endl;
#endif
    Trim( sKey );
    KeyIndexW::iterator itr = _find_key( sKey );
    if( itr != m_keys.end() )
    {
        delete *itr;
        m_keys.erase( itr );
    }
}

void CIniSectionW::RemoveKey( CIniKeyW* pKey )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::RemoveKey()" << std::endl;
#endif
    // No trim is done to improve efficiency since CIniKeyW* should already be trimmed
    KeyIndexW::iterator itr = _find_key( pKey->m_sKeyName );
    if( itr != m_keys.end() )
    {
        delete *itr;
        m_keys.erase( itr );
    }
}

CIniKeyW* CIniSectionW::AddKey( std::wstring sKeyName )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::AddKey()" << std::endl;
#endif
    Trim(sKeyName);
    KeyIndexW::const_iterator itr = _find_key( sKeyName );
    if( itr == m_keys.end() )
    {
        // Note constuctor doesn't trim the string so it is trimmed above
        CIniKeyW* pKey = new CIniKeyW( this , sKeyName );
        m_keys.insert(pKey);
        return pKey;
    }
    else
        return *itr;
}

bool CIniSectionW::SetSectionName( std::wstring sSectionName )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::SetSectionName()" << std::endl;
#endif
    Trim(sSectionName);
    // Does this already exist.
    if( m_pIniFile->_find_sec( sSectionName ) == m_pIniFile->m_sections.end() )
    {
#ifdef _CINIFILE_DEBUG
        std::wcout <<  L"Setting Section Name: [" << m_sSectionName <<  L"] --> [" << sSectionName << L"]" << std::endl;
#endif

        // Find the current section if one exists and remove it since we are renaming
        SecIndexW::iterator itr = m_pIniFile->_find_sec( m_sSectionName );

        // Just to be safe make sure the old section exists
        if( itr != m_pIniFile->m_sections.end() )
            m_pIniFile->m_sections.erase(itr);

        // Change name prior to ensure tree balance
        m_sSectionName = sSectionName;

        // Set the new map entry we know should not exist
        m_pIniFile->m_sections.insert(this);

        return true;
    }
    else
    {
#ifdef _CINIFILE_DEBUG
        std::wcout <<  L"Section existed could not rename" << std::endl;
#endif
        return false;
    }
}

std::wstring CIniSectionW::GetSectionName() const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::GetSectionName()" << std::endl;
#endif
    return m_sSectionName;
}

const KeyIndexW& CIniSectionW::GetKeys() const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::GetKeys()" << std::endl;
#endif
    return m_keys;
}

std::wstring CIniSectionW::GetKeyValue( std::wstring sKey ) const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::GetKeyValue()" << std::endl;
#endif
    std::wstring sValue;
    CIniKeyW* pKey = GetKey( sKey );
    if( pKey )
    {
        sValue = pKey->GetValue();
    }
    return sValue;
}

void CIniSectionW::SetKeyValue( std::wstring sKey, const std::wstring& sValue )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::SetKeyValue()" << std::endl;
#endif
    CIniKeyW* pKey = AddKey( sKey );
    if( pKey )
    {
        pKey->SetValue( sValue );
    }
}

// Returns a constant iterator to a key by name, string is not trimmed
KeyIndexW::const_iterator CIniSectionW::_find_key( const std::wstring& sKey ) const
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::_find_key() const" << std::endl;
#endif
    CIniKeyW bogus(NULL,sKey);
    return m_keys.find( &bogus );
}

// Returns an iterator to a key by name, string is not trimmed
KeyIndexW::iterator CIniSectionW::_find_key( const std::wstring& sKey )
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniSectionW::_find_key()" << std::endl;
#endif
    CIniKeyW bogus(NULL,sKey);
    return m_keys.find( &bogus );
}

// CIniSectionW function end here

// CIniKeyW Functions Start Here

CIniKeyW::CIniKeyW( CIniSectionW* pSection , const std::wstring& sKeyName ) : m_pSection(pSection) , m_sKeyName(sKeyName)
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniKeyW::CIniKeyW()" << std::endl;
#endif
}


CIniKeyW::~CIniKeyW()
{
#ifdef _CINIFILE_DEBUG
    std::wcout <<  L"CIniKeyW::~CIniKeyW()" << std::endl;
#endif
}

void CIniKeyW::SetValue( const std::wstring& sValue )
{
#ifdef _CINIFILE_DEBUG
    std::wcout << L"CIniKeyW::SetValue()" << std::endl;
#endif
    m_sValue = sValue;
}

std::wstring CIniKeyW::GetValue() const
{
#ifdef _CINIFILE_DEBUG
    std::wcout << L"CIniKeyW::GetValue()" << std::endl;
#endif
    return m_sValue;
}

bool CIniKeyW::SetKeyName( std::wstring sKeyName )
{
#ifdef _CINIFILE_DEBUG
    std::wcout << L"CIniKeyW::SetKeyName()" << std::endl;
#endif
    Trim( sKeyName );

    // Check for key name conflict
    if( m_pSection->_find_key( sKeyName ) == m_pSection->m_keys.end() )
    {
        KeyIndexW::iterator itr = m_pSection->_find_key( m_sKeyName );

        // Find the old map entry and remove it
        if( itr != m_pSection->m_keys.end() )
            m_pSection->m_keys.erase( itr );

        // Change name prior to ensure tree balance
        m_sKeyName = sKeyName;

        // Make the new map entry
        m_pSection->m_keys.insert(this);
        return true;
    }
    else
    {
#ifdef _CINIFILE_DEBUG
        std::wcout << L"Could not set key name, key by that name already exists!" << std::endl;
#endif
        return false;
    }
}

std::wstring CIniKeyW::GetKeyName() const
{
#ifdef _CINIFILE_DEBUG
    std::wcout << L"CIniKeyW::GetKeyName()" << std::endl;
#endif
    return m_sKeyName;
}

// End of CIniKeyW Functions
