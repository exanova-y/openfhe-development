//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

#include "lwe-pke.h"

#include "math/binaryuniformgenerator.h"
#include "math/discreteuniformgenerator.h"
#include "math/ternaryuniformgenerator.h"

namespace lbcrypto {
// the main rounding operation used in ModSwitch (as described in Section 3 of
// https://eprint.iacr.org/2014/816) The idea is that Round(x) = 0.5 + Floor(x)
NativeInteger LWEEncryptionScheme::RoundqQ(const NativeInteger& v, const NativeInteger& q,
                                           const NativeInteger& Q) const {
    return NativeInteger(static_cast<BasicInteger>(
                             std::floor(0.5 + v.ConvertToDouble() * q.ConvertToDouble() / Q.ConvertToDouble())))
        .Mod(q);
}

LWEPrivateKey LWEEncryptionScheme::KeyGen(usint size, const NativeInteger& modulus) const {
    TernaryUniformGeneratorImpl<NativeVector> tug;
    return std::make_shared<LWEPrivateKeyImpl>(LWEPrivateKeyImpl(tug.GenerateVector(size, modulus)));
}

LWEPrivateKey LWEEncryptionScheme::KeyGenGaussian(usint size, const NativeInteger& modulus) const {
    DiscreteGaussianGeneratorImpl<NativeVector> dgg;
    return std::make_shared<LWEPrivateKeyImpl>(LWEPrivateKeyImpl(dgg.GenerateVector(size, modulus)));
}

// size is the ring dimension N, modulus is the large Q used in RGSW encryption of bootstrapping.
LWEKeyPair LWEEncryptionScheme::KeyGenPair(const std::shared_ptr<LWECryptoParams>& params) const {
    int size              = params->GetN();
    NativeInteger modulus = params->GetQ();

    // generate secret vector skN of ring dimension N
    LWEPrivateKey skN = KeyGen(size, modulus);
    if (params->GetKeyDist() == GAUSSIAN) {
        skN = KeyGenGaussian(size, modulus);
    }
    else {
        skN = KeyGen(size, modulus);
    }
    // generate public key pkN corresponding to secret key skN
    auto pkN = PubKeyGen(params, skN);

    auto lweKeyPair = LWEKeyPairImpl(pkN, skN);

    // return the public key (A, v), private key sk pair
    return std::make_shared<LWEKeyPairImpl>(lweKeyPair);
}

// size is the ring dimension N, modulus is the large Q used in RGSW encryption of bootstrapping.
LWEPublicKey LWEEncryptionScheme::PubKeyGen(const std::shared_ptr<LWECryptoParams>& params,
                                            ConstLWEPrivateKey& skN) const {
    size_t dim            = params->GetN();
    NativeInteger modulus = params->GetQ();

    DiscreteUniformGeneratorImpl<NativeVector> dug;
    dug.SetModulus(modulus);
    std::vector<NativeVector> A(dim);

    // generate random matrix A of dimension N x N
    for (size_t i = 0; i < dim; i++) {
        NativeVector a = dug.GenerateVector(dim);
        A[i]           = std::move(a);
    }

    // generate error vector e
    DiscreteGaussianGeneratorImpl<NativeVector> dgg;
    NativeVector e = dgg.GenerateVector(dim, modulus);

    // compute v = As + e
    NativeVector v = e;

    NativeVector ske = skN->GetElement();
    NativeInteger mu = modulus.ComputeMu();

    for (size_t i = 0; i < dim; ++i) {
        v[i] = 0;
    }
    // todosara
    // NativeVector v (dim, modulus);
    std::cout << "here" << std::endl;
    for (size_t j = 0; j < dim; ++j) {
        for (size_t i = 0; i < dim; ++i) {
            v[j].ModAddEq(A[j][i].ModMulFast(ske[i], modulus, mu), modulus);
        }
    }

    std::cout << "here after" << std::endl;
    // public key A, v
    LWEPublicKeyImpl Av(A, v);

    return std::make_shared<LWEPublicKeyImpl>(Av);
}

LWEKeyPair LWEEncryptionScheme::MultipartyKeyGen(const std::vector<LWEPrivateKey>& privateKeyVec,
                                                 const std::shared_ptr<LWECryptoParams> params) {
    LWEKeyPair keyPair;

    // Private Key Generation
    auto sk = privateKeyVec[0];
    for (size_t i = 1; i < privateKeyVec.size(); i++) {
        *sk += *privateKeyVec[i];
    }

    // Public Key Generation
    auto pk = PubKeyGen(params, sk);

    auto lweKeyPair = LWEKeyPairImpl(pk, sk);

    // return the public key (A, v), private key sk pair
    return std::make_shared<LWEKeyPairImpl>(lweKeyPair);
}

LWEPublicKey LWEEncryptionScheme::MultipartyPubKeyGen(const LWEPrivateKey sk, const LWEPublicKey publicKey) {
    LWEKeyPair keyPair;

    auto A          = publicKey->GetA();
    NativeVector pv = publicKey->Getv();
    auto dim        = publicKey->GetLength();
    auto modulus    = publicKey->GetModulus();

    // generate error vector e
    DiscreteGaussianGeneratorImpl<NativeVector> dgg;
    NativeVector e = dgg.GenerateVector(dim, modulus);

    // auto sk = KeyGen(dim, modulus);
    // compute v = As + e
    NativeVector v = e;

    NativeVector ske = sk->GetElement();
    NativeInteger mu = modulus.ComputeMu();

    // todosara:
    // NativeVector v (dim, modulus);
    for (size_t i = 0; i < dim; ++i) {
        v[i] = 0;
    }

    for (size_t j = 0; j < dim; ++j) {
        for (size_t i = 0; i < dim; ++i) {
            v[j].ModAddEq(A[j][i].ModMulFast(ske[i], modulus, mu), modulus);
        }
    }

    // joint public key Asi + ei + prevkey
    for (size_t j = 0; j < dim; ++j) {
        v[j].ModAddEq(pv[j], modulus);
    }
    // public key A, v
    LWEPublicKeyImpl pki(A, v);

    // auto lweKeyPair = LWEKeyPairImpl(pk, sk);

    return std::make_shared<LWEPublicKeyImpl>(pki);
}

// classical LWE encryption
// a is a randomly uniform vector of dimension n; with integers mod q
// b = a*s + e + m floor(q/4) is an integer mod q
LWECiphertext LWEEncryptionScheme::Encrypt(const std::shared_ptr<LWECryptoParams>& params, ConstLWEPrivateKey& sk,
                                           LWEPlaintext m, LWEPlaintextModulus p, NativeInteger mod) const {
    if (mod % p != 0 && mod.ConvertToInt() & (1 == 0)) {
        std::string errMsg = "ERROR: ciphertext modulus q needs to be divisible by plaintext modulus p.";
        OPENFHE_THROW(not_implemented_error, errMsg);
    }

    NativeVector s = sk->GetElement();
    uint32_t n     = s.GetLength();
    s.SwitchModulus(mod);

    NativeInteger b = (m % p) * (mod / p) + params->GetDgg().GenerateInteger(mod);

    // #if defined(BINFHE_DEBUG)
    //    std::cout << b % mod << std::endl;
    //    std::cout << (m % p) * (mod / p) << std::endl;
    // #endif

    DiscreteUniformGeneratorImpl<NativeVector> dug;
    dug.SetModulus(mod);
    NativeVector a = dug.GenerateVector(n);

    NativeInteger mu = mod.ComputeMu();

    for (size_t i = 0; i < n; ++i) {
        b += a[i].ModMulFast(s[i], mod, mu);
    }

    auto ct = std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(std::move(a), b.Mod(mod)));
    ct->SetptModulus(p);
    return ct;
}

// classical public key LWE encryption
// a = As' + e' of dimension n; with integers mod q
// b = vs' + e" + m floor(q/4) is an integer mod q
LWECiphertext LWEEncryptionScheme::EncryptN(const std::shared_ptr<LWECryptoParams>& params, ConstLWEPublicKey& pk,
                                            LWEPlaintext m, LWEPlaintextModulus p, NativeInteger mod) const {
    if (mod % p != 0 && mod.ConvertToInt() & (1 == 0)) {
        std::string errMsg = "ERROR: ciphertext modulus q needs to be divisible by plaintext modulus p.";
        OPENFHE_THROW(not_implemented_error, errMsg);
    }
    NativeVector bp             = pk->Getv();
    std::vector<NativeVector> A = pk->GetA();

    uint32_t N = bp.GetLength();
    bp.SwitchModulus(mod);  // todo : this is probably not required

    auto dgg        = params->GetDgg();
    NativeInteger b = (m % p) * (mod / p) + dgg.GenerateInteger(mod);

    // #if defined(BINFHE_DEBUG)
    //    std::cout << b % mod << std::endl;
    //    std::cout << (m % p) * (mod / p) << std::endl;
    // #endif

    TernaryUniformGeneratorImpl<NativeVector> tug;
    NativeVector sp = tug.GenerateVector(N, mod);
    NativeVector ep = dgg.GenerateVector(N, mod);

    // compute a in the ciphertext (a, b)
    NativeVector a   = ep;
    NativeInteger mu = mod.ComputeMu();

    for (size_t j = 0; j < N; ++j) {
        // columnwise a = A_1s1 + ... + A_NsN
        a.ModAddEq(A[j].ModMul(sp[j]));
    }

    // compute b in ciphertext (a,b)
    for (size_t i = 0; i < N; ++i) {
        b.ModAddEq(bp[i].ModMulFast(sp[i], mod, mu), mod);
    }

    auto ct = std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(a, b));
    ct->SetptModulus(p);
    return ct;
}

// convert ciphertext with modulus Q and dimension N to ciphertext with modulus q and dimension n
LWECiphertext LWEEncryptionScheme::SwitchCTtoqn(const std::shared_ptr<LWECryptoParams>& params,
                                                ConstLWESwitchingKey& ksk, ConstLWECiphertext& ct) const {
    // Modulus switching to a middle step Q'
    auto ctMS = ModSwitch(params->GetqKS(), ct);
    // Key switching
    auto ctKS = KeySwitch(params, ksk, ctMS);
    // Modulus switching
    return ModSwitch(params->Getq(), ctKS);
}

// classical LWE decryption
// m_result = Round(4/q * (b - a*s))
void LWEEncryptionScheme::Decrypt(const std::shared_ptr<LWECryptoParams>& params, ConstLWEPrivateKey& sk,
                                  ConstLWECiphertext& ct, LWEPlaintext* result, LWEPlaintextModulus p) const {
    // TODO in the future we should add a check to make sure sk parameters match
    // the ct parameters

    // Create local variables to speed up the computations
    const NativeInteger& mod = ct->GetModulus();
    if (mod % (p * 2) != 0 && mod.ConvertToInt() & (1 == 0)) {
        std::string errMsg = "ERROR: ciphertext modulus q needs to be divisible by plaintext modulus p*2.";
        OPENFHE_THROW(not_implemented_error, errMsg);
    }

    NativeVector a   = ct->GetA();
    NativeVector s   = sk->GetElement();
    uint32_t n       = s.GetLength();
    NativeInteger mu = mod.ComputeMu();
    s.SwitchModulus(mod);
    NativeInteger inner(0);
    for (size_t i = 0; i < n; ++i) {
        inner += a[i].ModMulFast(s[i], mod, mu);
    }
    inner.ModEq(mod);

    NativeInteger r = ct->GetB();

    r.ModSubFastEq(inner, mod);

    // Alternatively, rounding can be done as
    // *result = (r.MultiplyAndRound(NativeInteger(4),q)).ConvertToInt();
    // But the method below is a more efficient way of doing the rounding
    // the idea is that Round(4/q x) = q/8 + Floor(4/q x)
    r.ModAddFastEq((mod / (p * 2)), mod);

    *result = ((NativeInteger(p) * r) / mod).ConvertToInt();

#if defined(WITH_NOISE_DEBUG)
    double error =
        (static_cast<double>(p) * (r.ConvertToDouble() - mod.ConvertToDouble() / (p * 2))) / mod.ConvertToDouble() -
        static_cast<double>(*result);
    std::cerr << error * mod.ConvertToDouble() / static_cast<double>(p) << std::endl;
#endif
}

LWECiphertext LWEEncryptionScheme::MultipartyDecryptLead(const std::shared_ptr<LWECryptoParams> params,
                                                         ConstLWEPrivateKey sk, ConstLWECiphertext ct,
                                                         const LWEPlaintextModulus& p) const {
    // Create local variables to speed up the computations
    const NativeInteger& mod = ct->GetModulus();
    if (mod % (p * 2) != 0 && mod.ConvertToInt() & (1 == 0)) {
        std::string errMsg = "ERROR: ciphertext modulus q needs to be divisible by plaintext modulus p*2.";
        OPENFHE_THROW(not_implemented_error, errMsg);
    }

    NativeVector a   = ct->GetA();
    NativeVector s   = sk->GetElement();
    uint32_t n       = s.GetLength();
    NativeInteger mu = mod.ComputeMu();
    s.SwitchModulus(mod);
    NativeInteger inner(0);
    for (size_t i = 0; i < n; ++i) {
        inner += a[i].ModMulFast(s[i], mod, mu);
    }
    inner.ModEq(mod);

    NativeInteger r = ct->GetB();

    r.ModSubFastEq(inner, mod);

    return std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(a, r));
}
LWECiphertext LWEEncryptionScheme::MultipartyDecryptMain(const std::shared_ptr<LWECryptoParams> params,
                                                         ConstLWEPrivateKey sk, ConstLWECiphertext ct,
                                                         const LWEPlaintextModulus& p) const {
    // TODO in the future we should add a check to make sure sk parameters match
    // the ct parameters

    // Create local variables to speed up the computations
    const NativeInteger& mod = ct->GetModulus();
    if (mod % (p * 2) != 0 && mod.ConvertToInt() & (1 == 0)) {
        std::string errMsg = "ERROR: ciphertext modulus q needs to be divisible by plaintext modulus p*2.";
        OPENFHE_THROW(not_implemented_error, errMsg);
    }

    NativeVector a   = ct->GetA();
    NativeVector s   = sk->GetElement();
    uint32_t n       = s.GetLength();
    NativeInteger mu = mod.ComputeMu();
    s.SwitchModulus(mod);
    NativeInteger inner(0);
    for (size_t i = 0; i < n; ++i) {
        inner += a[i].ModMulFast(s[i], mod, mu);
    }
    inner.ModEq(mod);

    NativeInteger r = inner;

    return std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(a, r));
}
void LWEEncryptionScheme::MultipartyDecryptFusion(const std::vector<LWECiphertext>& partialCiphertextVec,
                                                  LWEPlaintext* plaintext, const LWEPlaintextModulus& p) const {
    // const auto cryptoParams =
    //    std::dynamic_pointer_cast<CryptoParametersRLWE<Element>>(ciphertextVec[0]->GetCryptoParameters());
    const NativeInteger& mod = partialCiphertextVec[0]->GetModulus();
    auto cv0                 = partialCiphertextVec[0]->GetB();

    auto b = cv0;
    for (size_t i = 1; i < partialCiphertextVec.size(); i++) {
        auto cvi = partialCiphertextVec[i]->GetB();
        b.ModSubFastEq(cvi, mod);
    }

    // Alternatively, rounding can be done as
    // *result = (r.MultiplyAndRound(NativeInteger(4),q)).ConvertToInt();
    // But the method below is a more efficient way of doing the rounding
    // the idea is that Round(4/q x) = q/8 + Floor(4/q x)
    b.ModAddFastEq((mod / (p * 2)), mod);

    *plaintext = ((NativeInteger(p) * b) / mod).ConvertToInt();

    return;
}

void LWEEncryptionScheme::EvalAddEq(LWECiphertext& ct1, ConstLWECiphertext& ct2) const {
    ct1->GetA().ModAddEq(ct2->GetA());
    ct1->GetB().ModAddFastEq(ct2->GetB(), ct1->GetModulus());
}

void LWEEncryptionScheme::EvalAddConstEq(LWECiphertext& ct, NativeInteger cnst) const {
    ct->GetB().ModAddFastEq(cnst, ct->GetModulus());
}

void LWEEncryptionScheme::EvalSubEq(LWECiphertext& ct1, ConstLWECiphertext& ct2) const {
    ct1->GetA().ModSubEq(ct2->GetA());
    ct1->GetB().ModSubFastEq(ct2->GetB(), ct1->GetModulus());
}

void LWEEncryptionScheme::EvalSubEq2(ConstLWECiphertext& ct1, LWECiphertext& ct2) const {
    ct2->GetA() = ct1->GetA().ModSub(ct2->GetA());
    ct2->GetB() = ct1->GetB().ModSubFast(ct2->GetB(), ct1->GetModulus());
}

void LWEEncryptionScheme::EvalSubConstEq(LWECiphertext& ct, NativeInteger cnst) const {
    ct->GetB().ModSubFastEq(cnst, ct->GetModulus());
}

void LWEEncryptionScheme::EvalMultConstEq(LWECiphertext& ct1, NativeInteger cnst) const {
    ct1->GetA().ModMulEq(cnst);
    ct1->GetB().ModMulFastEq(cnst, ct1->GetModulus());
}

// Modulus switching - directly applies the scale-and-round operation RoundQ
LWECiphertext LWEEncryptionScheme::ModSwitch(NativeInteger q, ConstLWECiphertext& ctQ) const {
    auto n = ctQ->GetLength();
    auto Q = ctQ->GetModulus();
    NativeVector a(n, q);
    for (size_t i = 0; i < n; ++i)
        a[i] = RoundqQ(ctQ->GetA()[i], q, Q);
    return std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(std::move(a), RoundqQ(ctQ->GetB(), q, Q)));
}

// Switching key as described in Section 3 of https://eprint.iacr.org/2014/816
LWESwitchingKey LWEEncryptionScheme::KeySwitchGen(const std::shared_ptr<LWECryptoParams> params, ConstLWEPrivateKey sk,
                                                  ConstLWEPrivateKey skN) const {
    // Create local copies of main variables
    uint32_t n        = params->Getn();
    uint32_t N        = params->GetN();
    NativeInteger qKS = params->GetqKS();
    uint32_t baseKS   = params->GetBaseKS();
    // Number of digits in representing numbers mod qKS
    uint32_t digitCount = (uint32_t)std::ceil(log(qKS.ConvertToDouble()) / log(static_cast<double>(baseKS)));
    std::vector<NativeInteger> digitsKS;
    // Populate digits
    NativeInteger value = 1;
    for (size_t i = 0; i < digitCount; ++i) {
        digitsKS.push_back(value);
        value *= baseKS;
    }
    // newSK stores negative values using modulus q
    // we need to switch to modulus Q
    NativeVector sv = sk->GetElement();
    sv.SwitchModulus(qKS);

    NativeVector svN = skN->GetElement();
    svN.SwitchModulus(qKS);
    //    NativeVector oldSK(oldSKlargeQ.GetLength(), qKS);
    //    for (size_t i = 0; i < oldSK.GetLength(); i++) {
    //        if ((oldSKlargeQ[i] == 0) || (oldSKlargeQ[i] == 1)) {
    //            oldSK[i] = oldSKlargeQ[i];
    //        }
    //        else {
    //            oldSK[i] = qKS - 1;
    //        }
    //    }

    DiscreteUniformGeneratorImpl<NativeVector> dug;
    dug.SetModulus(qKS);

    NativeInteger mu = qKS.ComputeMu();

    std::vector<std::vector<std::vector<NativeVector>>> resultVecA(N);
    std::vector<std::vector<std::vector<NativeInteger>>> resultVecB(N);

#pragma omp parallel for
    for (size_t i = 0; i < N; ++i) {
        std::vector<std::vector<NativeVector>> vector1A(baseKS);
        std::vector<std::vector<NativeInteger>> vector1B(baseKS);
        for (size_t j = 0; j < baseKS; ++j) {
            std::vector<NativeVector> vector2A(digitCount);
            std::vector<NativeInteger> vector2B(digitCount);
            for (size_t k = 0; k < digitCount; ++k) {
                // NativeInteger b =
                //    (params->GetDggKS().GenerateInteger(qKS)).ModAdd(svN[i].ModMul(j * digitsKS[k], qKS), qKS);
                // todosara
                NativeInteger b = svN[i].ModMul(j * digitsKS[k], qKS);

                NativeVector a = dug.GenerateVector(n);

#if NATIVEINT == 32
                for (size_t ai = 0; ai < n; ++ai) {
                    b.ModAddFastEq(a[ai].ModMulFast(sv[ai], qKS, mu), qKS);
                }
                b.ModEq(qKS);
#else
                for (size_t ai = 0; ai < n; ++ai) {
                    b.ModAddFastEq(a[ai].ModMulFast(sv[ai], qKS, mu), qKS);
                }
                b.ModEq(qKS);
#endif

                vector2A[k] = std::move(a);
                vector2B[k] = std::move(b);
            }
            vector1A[j] = std::move(vector2A);
            vector1B[j] = std::move(vector2B);
        }
        resultVecA[i] = std::move(vector1A);
        resultVecB[i] = std::move(vector1B);
    }

    return std::make_shared<LWESwitchingKeyImpl>(LWESwitchingKeyImpl(resultVecA, resultVecB));
}

// Switching key as described in Section 3 of https://eprint.iacr.org/2014/816
LWESwitchingKey LWEEncryptionScheme::MultiPartyKeySwitchGen(const std::shared_ptr<LWECryptoParams> params,
                                                            ConstLWEPrivateKey sk, ConstLWEPrivateKey skN,
                                                            LWESwitchingKey prevkskey) const {
    // Create local copies of main variables
    uint32_t n        = params->Getn();
    uint32_t N        = params->GetN();
    NativeInteger qKS = params->GetqKS();
    uint32_t baseKS   = params->GetBaseKS();
    // Number of digits in representing numbers mod qKS
    uint32_t digitCount = (uint32_t)std::ceil(log(qKS.ConvertToDouble()) / log(static_cast<double>(baseKS)));
    std::vector<NativeInteger> digitsKS;
    // Populate digits
    NativeInteger value = 1;
    for (size_t i = 0; i < digitCount; ++i) {
        digitsKS.push_back(value);
        value *= baseKS;
    }
    // newSK stores negative values using modulus q
    // we need to switch to modulus Q
    NativeVector sv = sk->GetElement();
    sv.SwitchModulus(qKS);

    NativeVector svN = skN->GetElement();
    svN.SwitchModulus(qKS);

    NativeInteger mu = qKS.ComputeMu();

    std::vector<std::vector<std::vector<NativeInteger>>> resultVecB(N);
    auto aprevkskey = prevkskey->GetElementsA();
    auto bprevkskey = prevkskey->GetElementsB();

#pragma omp parallel for
    for (size_t i = 0; i < N; ++i) {
        std::vector<std::vector<NativeInteger>> vector1B(baseKS);
        for (size_t j = 0; j < baseKS; ++j) {
            std::vector<NativeInteger> vector2B(digitCount);
            for (size_t k = 0; k < digitCount; ++k) {
                // NativeInteger b =
                //    (params->GetDggKS().GenerateInteger(qKS)).ModAdd(svN[i].ModMul(j * digitsKS[k], qKS), qKS);
                // todosara
                NativeInteger b = svN[i].ModMul(j * digitsKS[k], qKS);

#if NATIVEINT == 32
                for (size_t ai = 0; ai < n; ++ai) {
                    b.ModAddFastEq(aprevkskey[i][j][k][ai].ModMulFast(sv[i], qKS, mu), qKS);
                }
                b.ModAddEq(bprevkskey[i][j][k], qKS);
#else
                for (size_t ai = 0; ai < n; ++ai) {
                    b.ModAddFastEq(aprevkskey[i][j][k][ai].ModMulFast(sv[ai], qKS, mu), qKS);
                }
                b.ModAddEq(bprevkskey[i][j][k], qKS);
#endif
                vector2B[k] = std::move(b);
            }
            vector1B[j] = std::move(vector2B);
        }
        resultVecB[i] = std::move(vector1B);
    }

    return std::make_shared<LWESwitchingKeyImpl>(LWESwitchingKeyImpl(aprevkskey, resultVecB));
}

// the key switching operation as described in Section 3 of
// https://eprint.iacr.org/2014/816
LWECiphertext LWEEncryptionScheme::KeySwitch(const std::shared_ptr<LWECryptoParams>& params, ConstLWESwitchingKey& K,
                                             ConstLWECiphertext& ctQN) const {
    const size_t n(params->Getn());
    const size_t N(params->GetN());
    NativeInteger Q(params->GetqKS());
    NativeInteger::Integer baseKS(params->GetBaseKS());
    const auto digitCount = static_cast<size_t>(std::ceil(log(Q.ConvertToDouble()) / log(static_cast<double>(baseKS))));

    NativeVector a(n, Q);
    NativeInteger b(ctQN->GetB());
    for (size_t i = 0; i < N; ++i) {
        auto& refA = K->GetElementsA()[i];
        auto& refB = K->GetElementsB()[i];
        NativeInteger::Integer atmp(ctQN->GetA(i).ConvertToInt());
        for (size_t j = 0; j < digitCount; ++j) {
            const auto a0 = (atmp % baseKS);
            atmp /= baseKS;
            b.ModSubFastEq(refB[a0][j], Q);
            auto& refAj = refA[a0][j];
            for (size_t k = 0; k < n; ++k)
                a[k].ModSubFastEq(refAj[k], Q);
        }
    }
    return std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(std::move(a), b));
}

// noiseless LWE embedding
// a is a zero vector of dimension n; with integers mod q
// b = m floor(q/4) is an integer mod q
LWECiphertext LWEEncryptionScheme::NoiselessEmbedding(const std::shared_ptr<LWECryptoParams>& params,
                                                      LWEPlaintext m) const {
    NativeInteger q(params->Getq());
    NativeInteger b(m * (q >> 2));
    NativeVector a(params->Getn(), q);
    return std::make_shared<LWECiphertextImpl>(LWECiphertextImpl(std::move(a), b));
}

};  // namespace lbcrypto
