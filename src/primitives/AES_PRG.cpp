//#include "stdafx.h"
#include "../../include/primitives/AES_PRG.hpp"

/*
unsigned char AES_PRG::m_defualtkey[16] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };

unsigned char AES_PRG::m_defaultiv[16] = { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };
*/

AES_PRG::AES_PRG(int cahchedSize) : AES_PRG((byte*)m_defualtkey, (byte*)m_defaultiv, cahchedSize) { }


AES_PRG::AES_PRG(byte *key, int cahchedSize) : AES_PRG(key, (byte*)m_defaultiv, cahchedSize) { }

AES_PRG::AES_PRG(byte *key, byte *iv, int cahchedSize)
{
    m_secretKey = new SecretKey(key, 16, "AES");
    m_iv = iv;
    m_cahchedSize = cahchedSize;
    m_cachedRandomsIdx = m_cahchedSize;
    EVP_CIPHER_CTX* temp = new EVP_CIPHER_CTX();
    m_enc = shared_ptr<EVP_CIPHER_CTX>(temp, EVP_CIPHER_CTX_DELETER::deleter);
    EVP_CIPHER_CTX_init(m_enc.get());
    EVP_EncryptInit(m_enc.get(), EVP_aes_128_ecb(), m_secretKey->getEncoded().data(), m_iv);
    m_cachedRandoms = nullptr;

#ifndef _WIN32
    m_cachedRandoms = (byte*)memalign(m_cahchedSize * 16, 16);
#else
    m_cachedRandoms = (byte*)_aligned_malloc(m_cahchedSize, 16);
#endif

    assert(m_cachedRandoms != nullptr);
    m_isKeySet = true;
    prepare(1);
}

AES_PRG::AES_PRG(AES_PRG && old):
		m_iv(old.m_iv), m_cahchedSize(old.m_cahchedSize), m_cachedRandomsIdx(old.m_cachedRandomsIdx), 
		m_enc(old.m_enc), m_isKeySet(old.m_isKeySet), m_secretKey(old.m_secretKey),  m_cachedRandoms(old.m_cachedRandoms)
{
    old.m_secretKey = nullptr;
    old.m_cachedRandoms = nullptr;
    old.m_iv = nullptr;
}

AES_PRG::AES_PRG(AES_PRG & other): m_iv(nullptr), m_cahchedSize(other.m_cahchedSize),
                                   m_cachedRandomsIdx(other.m_cachedRandomsIdx),  m_enc(other.m_enc)
{
    //secret key
    byte* tempKey = &vector<byte>()[0];
    m_secretKey = new SecretKey(tempKey, 16, "AES");
    *m_secretKey = *other.m_secretKey;

    //m_cachedRandoms
#ifndef _WIN32
    m_cachedRandoms = (byte*)memalign(m_cahchedSize * 16, 16);
#else
    m_cachedRandoms = (byte*)_aligned_malloc(m_cahchedSize, 16);
#endif
    assert(m_cachedRandoms != nullptr);
    memcpy(m_cachedRandoms, other.m_cachedRandoms, m_cahchedSize);
}

AES_PRG::~AES_PRG()
{
    delete m_secretKey;

    if (m_cachedRandoms != nullptr)
    {
#ifndef _WIN32
        free(m_cachedRandoms);
#else
        _aligned_free(m_cachedRandoms);
#endif
        m_cachedRandoms = nullptr;
    }

}

AES_PRG & AES_PRG::operator=(AES_PRG && other)
{
    //copy values
    m_enc = other.m_enc;
    m_secretKey = other.m_secretKey;
    m_cahchedSize = other.m_cahchedSize;
    m_cachedRandoms = other.m_cachedRandoms;
    m_iv = other.m_iv;
    m_cachedRandomsIdx = other.m_cachedRandomsIdx;
    m_isKeySet = other.m_isKeySet;

    //set other values to null
    other.m_secretKey = nullptr;
    other.m_cachedRandoms = nullptr;
    other.m_iv = nullptr;

    return *this;
}

AES_PRG & AES_PRG::operator=(AES_PRG & other)
{
    //secret key
    byte* tempKey = &vector<byte>()[0];
    m_secretKey = new SecretKey(tempKey, 16, "AES");
    *m_secretKey = *other.m_secretKey;

    m_iv = nullptr;
    m_cahchedSize = other.m_cahchedSize;
    m_cachedRandomsIdx = other.m_cachedRandomsIdx;
    m_isKeySet = other.m_isKeySet;
    m_enc = other.m_enc;

    //m_cachedRandoms
#ifndef _WIN32
    m_cachedRandoms = (byte*)memalign(m_cahchedSize * 16, 16);
#else
    m_cachedRandoms = (byte*)_aligned_malloc(m_cahchedSize, 16);
#endif

    assert(m_cachedRandoms != nullptr);
    memcpy(m_cachedRandoms, other.m_cachedRandoms, m_cahchedSize);

    return *this;
}


SecretKey AES_PRG::generateKey(int keySize)
{

    vector<byte> genBytes(m_secretKey->getEncoded().data(), m_secretKey->getEncoded().data() + keySize);
    SecretKey generatedKey(genBytes.data(),keySize,"AES");
    return generatedKey;

}

SecretKey AES_PRG::generateKey(AlgorithmParameterSpec keyParams)
{
    throw NotImplementedException("To generate a key use generateKey with size parameter");
}


string AES_PRG::getAlgorithmName()
{
    return "AES";
}

bool AES_PRG::isKeySet()
{
    return m_isKeySet;
}

void AES_PRG::setKey(SecretKey secretKey)
{
    //m_secretKey = &secretKey;
}

void AES_PRG::getPRGBytes(vector<byte> & outBytes, int outOffset, int outLen)
{
    for (int i = outOffset; i < (outOffset + outLen); i++)
    {
        outBytes[i] = m_cachedRandoms[m_cachedRandomsIdx];
        updateCachedRandomsIdx(1);
    }
}

byte * AES_PRG::getRandomBytes()
{
    byte *ret = m_cachedRandoms + m_cachedRandomsIdx;
    updateCachedRandomsIdx(1);

    return ret;

}


vector<byte> AES_PRG::getPRGBitsBytes(int size)
{
    vector<byte> prg(size);
    int byteNum = (size / 8) + 1;
    byte* data = m_cachedRandoms + m_cachedRandomsIdx;
    updateCachedRandomsIdx(byteNum);

    vector<bitset<8>> bits(byteNum);
    int idx = 0;
    for (int i = 0; i < byteNum; i++)
    {
        bits[i] = data[i];
        for (int j = 0; j < 8 && idx < size; j++, idx++)
        {
            if (bits[i][j] == 0)
                prg[idx] = 0;
            else
                prg[idx] = 1;
        }
    }

    return prg;
}

vector<byte> AES_PRG::getRandomBytes(int size)
{
    if (m_cachedRandomsIdx + size >= m_cahchedSize)
        prepare(0);
    byte* start = m_cachedRandoms + m_cachedRandomsIdx;
    updateCachedRandomsIdx(size);
    byte* end = m_cachedRandoms + m_cachedRandomsIdx;
    vector<byte> data(start, end);
    return data;
}


void AES_PRG::prepare(int isPlanned)
{
    int actual;
    byte *ctr;
#ifndef _WIN32
    ctr = (byte*)memalign(m_cahchedSize * 16, 16);
#else
    ctr = (byte*)_aligned_malloc(m_cahchedSize, 16);
#endif
    EVP_EncryptUpdate(m_enc.get(), m_cachedRandoms, &actual, ctr, m_cahchedSize);
    m_cachedRandomsIdx = 0;

#ifndef _WIN32
    free(ctr);
#else
    _aligned_free(ctr);
#endif
}

block* AES_PRG::getRandomBytesBlock(int size)
{
    block *data;

#ifndef _WIN32
    data = (block*)memalign(m_cahchedSize * 16, 16);
#else
    data = (block *)_aligned_malloc(sizeof(block) * size, 16);
#endif

    if (m_cachedRandomsIdx + size * 16 >= m_cahchedSize)
        prepare(0);

    auto start = m_cachedRandoms + m_cachedRandomsIdx;
    memcpy(data, start, size * 16);

    updateCachedRandomsIdx(size * 16);

    return data;

}

void AES_PRG::updateCachedRandomsIdx(int size)
{

    m_cachedRandomsIdx += size;
    if (m_cachedRandomsIdx >= m_cahchedSize)
        prepare(0);
}

uint32_t AES_PRG::getRandom()
{
    if (m_cachedRandomsIdx + 4 >= m_cahchedSize)
        prepare(0);
    auto temp = (uint32_t*)(m_cachedRandoms + m_cachedRandomsIdx) ;
    updateCachedRandomsIdx(4);

    return  *temp;
}