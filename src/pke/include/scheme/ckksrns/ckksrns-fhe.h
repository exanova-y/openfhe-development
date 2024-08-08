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

#ifndef LBCRYPTO_CRYPTO_CKKSRNS_FHE_H
#define LBCRYPTO_CRYPTO_CKKSRNS_FHE_H

#include "constants.h"
#include "encoding/plaintext-fwd.h"
#include "schemerns/rns-fhe.h"
#include "scheme/ckksrns/ckksrns-utils.h"
#include "utils/caller_info.h"
#include "math/hal/basicint.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @namespace lbcrypto
 * The namespace of lbcrypto
 */
namespace lbcrypto {

class CKKSBootstrapPrecom {
public:
    CKKSBootstrapPrecom() {}

    CKKSBootstrapPrecom(const CKKSBootstrapPrecom& rhs) {
        m_dim1         = rhs.m_dim1;
        m_slots        = rhs.m_slots;
        m_paramsEnc    = rhs.m_paramsEnc;
        m_paramsDec    = rhs.m_paramsDec;
        m_U0Pre        = rhs.m_U0Pre;
        m_U0hatTPre    = rhs.m_U0hatTPre;
        m_U0PreFFT     = rhs.m_U0PreFFT;
        m_U0hatTPreFFT = rhs.m_U0hatTPreFFT;
    }

    CKKSBootstrapPrecom(CKKSBootstrapPrecom&& rhs) {
        m_dim1         = rhs.m_dim1;
        m_slots        = rhs.m_slots;
        m_paramsEnc    = std::move(rhs.m_paramsEnc);
        m_paramsDec    = std::move(rhs.m_paramsDec);
        m_U0Pre        = std::move(rhs.m_U0Pre);
        m_U0hatTPre    = std::move(rhs.m_U0hatTPre);
        m_U0PreFFT     = std::move(rhs.m_U0PreFFT);
        m_U0hatTPreFFT = std::move(rhs.m_U0hatTPreFFT);
    }

    virtual ~CKKSBootstrapPrecom() {}
    // the inner dimension in the baby-step giant-step strategy
    uint32_t m_dim1 = 0;

    // number of slots for which the bootstrapping is performed
    uint32_t m_slots = 0;

    // level budget for homomorphic encoding, number of layers to collapse in one level,
    // number of layers remaining to be collapsed in one level to have exactly the number
    // of levels specified in the level budget, the number of rotations in one level,
    // the baby step and giant step in the baby-step giant-step strategy, the number of
    // rotations in the remaining level, the baby step and giant step in the baby-step
    // giant-step strategy for the remaining level
    std::vector<int32_t> m_paramsEnc = std::vector<int32_t>(CKKS_BOOT_PARAMS::TOTAL_ELEMENTS, 0);

    // level budget for homomorphic decoding, number of layers to collapse in one level,
    // number of layers remaining to be collapsed in one level to have exactly the number
    // of levels specified in the level budget, the number of rotations in one level,
    // the baby step and giant step in the baby-step giant-step strategy, the number of
    // rotations in the remaining level, the baby step and giant step in the baby-step
    // giant-step strategy for the remaining level
    std::vector<int32_t> m_paramsDec = std::vector<int32_t>(CKKS_BOOT_PARAMS::TOTAL_ELEMENTS, 0);

    // Linear map U0; used in decoding
    std::vector<ConstPlaintext> m_U0Pre;

    // Conj(U0^T); used in encoding
    std::vector<ConstPlaintext> m_U0hatTPre;

    // coefficients corresponding to U0; used in decoding
    std::vector<std::vector<ConstPlaintext>> m_U0PreFFT;

    // coefficients corresponding to conj(U0^T); used in encoding
    std::vector<std::vector<ConstPlaintext>> m_U0hatTPreFFT;

    template <class Archive>
    void save(Archive& ar) const {
        ar(cereal::make_nvp("dim1_Enc", m_dim1));
        ar(cereal::make_nvp("dim1_Dec", m_paramsDec[CKKS_BOOT_PARAMS::GIANT_STEP]));
        ar(cereal::make_nvp("slots", m_slots));
        ar(cereal::make_nvp("lEnc", m_paramsEnc[CKKS_BOOT_PARAMS::LEVEL_BUDGET]));
        ar(cereal::make_nvp("lDec", m_paramsDec[CKKS_BOOT_PARAMS::LEVEL_BUDGET]));
    }

    template <class Archive>
    void load(Archive& ar) {
        ar(cereal::make_nvp("dim1_Enc", m_dim1));
        ar(cereal::make_nvp("dim1_Dec", m_paramsDec[CKKS_BOOT_PARAMS::GIANT_STEP]));
        ar(cereal::make_nvp("slots", m_slots));
        ar(cereal::make_nvp("lEnc", m_paramsEnc[CKKS_BOOT_PARAMS::LEVEL_BUDGET]));
        ar(cereal::make_nvp("lDec", m_paramsDec[CKKS_BOOT_PARAMS::LEVEL_BUDGET]));
    }
};

class FHECKKSRNS : public FHERNS {
    using ParmType = typename DCRTPoly::Params;

public:
    virtual ~FHECKKSRNS() {}

    //------------------------------------------------------------------------------
    // Bootstrap Wrapper
    //------------------------------------------------------------------------------

    void EvalBootstrapSetup(const CryptoContextImpl<DCRTPoly>& cc, std::vector<uint32_t> levelBudget,
                            std::vector<uint32_t> dim1, uint32_t slots, uint32_t correctionFactor,
                            bool precompute) override;

    std::shared_ptr<std::map<usint, EvalKey<DCRTPoly>>> EvalBootstrapKeyGen(const PrivateKey<DCRTPoly> privateKey,
                                                                            uint32_t slots) override;

    void EvalBootstrapPrecompute(const CryptoContextImpl<DCRTPoly>& cc, uint32_t slots) override;

    Ciphertext<DCRTPoly> EvalBootstrap(ConstCiphertext<DCRTPoly> ciphertext, uint32_t numIterations,
                                       uint32_t precision) const override;

    //------------------------------------------------------------------------------
    // Find Rotation Indices
    //------------------------------------------------------------------------------

    std::vector<int32_t> FindBootstrapRotationIndices(uint32_t slots, uint32_t M);

    std::vector<int32_t> FindLinearTransformRotationIndices(uint32_t slots, uint32_t M);

    std::vector<int32_t> FindCoeffsToSlotsRotationIndices(uint32_t slots, uint32_t M);

    std::vector<int32_t> FindSlotsToCoeffsRotationIndices(uint32_t slots, uint32_t M);

    //------------------------------------------------------------------------------
    // Precomputations for CoeffsToSlots and SlotsToCoeffs
    //------------------------------------------------------------------------------

    std::vector<ConstPlaintext> EvalLinearTransformPrecompute(const CryptoContextImpl<DCRTPoly>& cc,
                                                              const std::vector<std::vector<std::complex<double>>>& A,
                                                              double scale = 1, uint32_t L = 0) const;

    std::vector<ConstPlaintext> EvalLinearTransformPrecompute(const CryptoContextImpl<DCRTPoly>& cc,
                                                              const std::vector<std::vector<std::complex<double>>>& A,
                                                              const std::vector<std::vector<std::complex<double>>>& B,
                                                              uint32_t orientation = 0, double scale = 1,
                                                              uint32_t L = 0) const;

    std::vector<std::vector<ConstPlaintext>> EvalCoeffsToSlotsPrecompute(const CryptoContextImpl<DCRTPoly>& cc,
                                                                         const std::vector<std::complex<double>>& A,
                                                                         const std::vector<uint32_t>& rotGroup,
                                                                         bool flag_i, double scale = 1,
                                                                         uint32_t L = 0) const;

    std::vector<std::vector<ConstPlaintext>> EvalSlotsToCoeffsPrecompute(const CryptoContextImpl<DCRTPoly>& cc,
                                                                         const std::vector<std::complex<double>>& A,
                                                                         const std::vector<uint32_t>& rotGroup,
                                                                         bool flag_i, double scale = 1,
                                                                         uint32_t L = 0) const;

    //------------------------------------------------------------------------------
    // EVALUATION: CoeffsToSlots and SlotsToCoeffs
    //------------------------------------------------------------------------------

    Ciphertext<DCRTPoly> EvalLinearTransform(const std::vector<ConstPlaintext>& A, ConstCiphertext<DCRTPoly> ct) const;

    Ciphertext<DCRTPoly> EvalCoeffsToSlots(const std::vector<std::vector<ConstPlaintext>>& A,
                                           ConstCiphertext<DCRTPoly> ctxt) const;

    Ciphertext<DCRTPoly> EvalSlotsToCoeffs(const std::vector<std::vector<ConstPlaintext>>& A,
                                           ConstCiphertext<DCRTPoly> ctxt) const;

    //------------------------------------------------------------------------------
    // SERIALIZATION
    //------------------------------------------------------------------------------

    template <class Archive>
    void save(Archive& ar) const {
        ar(cereal::base_class<FHERNS>(this));
        ar(cereal::make_nvp("paramMap", m_bootPrecomMap));
        ar(cereal::make_nvp("corFactor", m_correctionFactor));
    }

    template <class Archive>
    void load(Archive& ar) {
        ar(cereal::base_class<FHERNS>(this));
        ar(cereal::make_nvp("paramMap", m_bootPrecomMap));
        ar(cereal::make_nvp("corFactor", m_correctionFactor));
    }

    // To be deprecated; left for backwards compatibility
    static uint32_t GetBootstrapDepth(uint32_t approxModDepth, const std::vector<uint32_t>& levelBudget,
                                      SecretKeyDist secretKeyDist);

    static uint32_t GetBootstrapDepth(const std::vector<uint32_t>& levelBudget, SecretKeyDist secretKeyDist);

    std::string SerializedObjectName() const {
        return "FHECKKSRNS";
    }

private:
    //------------------------------------------------------------------------------
    // Auxiliary Bootstrap Functions
    //------------------------------------------------------------------------------
    uint32_t GetBootstrapDepthInternal(uint32_t approxModDepth, const std::vector<uint32_t>& levelBudget,
                                       const CryptoContextImpl<DCRTPoly>& cc);
    static uint32_t GetModDepthInternal(SecretKeyDist secretKeyDist);

    void AdjustCiphertext(Ciphertext<DCRTPoly>& ciphertext, double correction) const;

    void ApplyDoubleAngleIterations(Ciphertext<DCRTPoly>& ciphertext, uint32_t numIt) const;

    Plaintext MakeAuxPlaintext(const CryptoContextImpl<DCRTPoly>& cc, const std::shared_ptr<ParmType> params,
                               const std::vector<std::complex<double>>& value, size_t noiseScaleDeg, uint32_t level,
                               usint slots) const;

    Ciphertext<DCRTPoly> EvalMultExt(ConstCiphertext<DCRTPoly> ciphertext, ConstPlaintext plaintext) const;

    void EvalAddExtInPlace(Ciphertext<DCRTPoly>& ciphertext1, ConstCiphertext<DCRTPoly> ciphertext2) const;

    Ciphertext<DCRTPoly> EvalAddExt(ConstCiphertext<DCRTPoly> ciphertext1, ConstCiphertext<DCRTPoly> ciphertext2) const;

    EvalKey<DCRTPoly> ConjugateKeyGen(const PrivateKey<DCRTPoly> privateKey) const;

    Ciphertext<DCRTPoly> Conjugate(ConstCiphertext<DCRTPoly> ciphertext,
                                   const std::map<usint, EvalKey<DCRTPoly>>& evalKeys) const;

    /**
   * Set modulus and recalculates the vector values to fit the modulus
   *
   * @param &vec input vector
   * @param &bigValue big bound of the vector values.
   * @param &modulus modulus to be set for vector.
   */
    void FitToNativeVector(uint32_t ringDim, const std::vector<int64_t>& vec, int64_t bigBound,
                           NativeVector* nativeVec) const;

#if NATIVEINT == 128 && !defined(__EMSCRIPTEN__)
    /**
   * Set modulus and recalculates the vector values to fit the modulus
   *
   * @param &vec input vector
   * @param &bigValue big bound of the vector values.
   * @param &modulus modulus to be set for vector.
   */
    void FitToNativeVector(uint32_t ringDim, const std::vector<int128_t>& vec, int128_t bigBound,
                           NativeVector* nativeVec) const;
#endif

    const uint32_t K_SPARSE  = 28;   // upper bound for the number of overflows in the sparse secret case
    const uint32_t K_UNIFORM = 512;  // upper bound for the number of overflows in the uniform secret case
    static const uint32_t R_UNIFORM =
        6;  // number of double-angle iterations in CKKS bootstrapping. Must be static because it is used in a static function.
    static const uint32_t R_SPARSE =
        3;  // number of double-angle iterations in CKKS bootstrapping. Must be static because it is used in a static function.
    uint32_t m_correctionFactor = 0;  // correction factor, which we scale the message by to improve precision

    // key tuple is dim1, levelBudgetEnc, levelBudgetDec
    std::map<uint32_t, std::shared_ptr<CKKSBootstrapPrecom>> m_bootPrecomMap;

    // Chebyshev series coefficients for the SPARSE case
    static const inline std::vector<double> g_coefficientsSparse{0.051667950339505692,-0.051331021411262792,0.054391145603358268,-0.045559941622459216,0.061642235519550802,-0.032479052974230690,0.070257571899204785,-0.010115373259445478,0.074014032428612139,0.021297157725027743,0.063845388651374568,0.055168188383325954,0.031650992415060121,0.075317835969809566,-0.020293566474452410,0.060245336022068822,-0.068235281605174836,0.0023254300981702058,-0.070332535876492297,-0.064837194371420742,-0.0049778560004883976,-0.070118863627936762,0.073140490252120063,0.015246053839852879,0.054537439879144070,0.084685278962595631,-0.057779899737632208,0.0049869879560770916,-0.064923233796776794,-0.091452978793089529,0.075776971345408659,0.029149816828457239,0.027837151147861859,0.076407632367427245,-0.10593002504560581,-0.11466428818827884,0.10697950466930695,0.089651515543433494,-0.068998438201839998,-0.049446756192184235,0.033307605480156918,0.021234177870771417,-0.012879227585062743,-0.0074629203963833763,0.0041453420323998430,0.0022134375074726550,-0.0011388507756961365,-0.00056578635857651818,0.00027189645704615731,0.00012659228348801623,-0.000057184076013433917,-0.000025093185722304338,0.000010708912907131761,4.4493010938744704*10e-6,-1.8014740230072435*10e-6,-7.1116258059986816*10e-7,2.7475174030438802*10e-7,1.0157509384824620*10e-7,-4.2817427341919936*10e-8};

    // Chebyshev series coefficients for the OPTIMIZED/uniform case
    static const inline std::vector<double> g_coefficientsUniform{
        0.15421426400235561,    -0.0037671538417132409,  0.16032011744533031,      -0.0034539657223742453,
        0.17711481926851286,    -0.0027619720033372291,  0.19949802549604084,      -0.0015928034845171929,
        0.21756948616367638,    0.00010729951647566607,  0.21600427371240055,      0.0022171399198851363,
        0.17647500259573556,    0.0042856217194480991,   0.086174491919472254,     0.0054640252312780444,
        -0.046667988130649173,  0.0047346914623733714,   -0.17712686172280406,     0.0016205080004247200,
        -0.22703114241338604,   -0.0028145845916205865,  -0.13123089730288540,     -0.0056345646688793190,
        0.078818395388692147,   -0.0037868875028868542,  0.23226434602675575,      0.0021116338645426574,
        0.13985510526186795,    0.0059365649669377071,   -0.13918475289368595,     0.0018580676740836374,
        -0.23254376365752788,   -0.0054103844866927788,  0.056840618403875359,     -0.0035227192748552472,
        0.25667909012207590,    0.0055029673963982112,   -0.073334392714092062,    0.0027810273357488265,
        -0.24912792167850559,   -0.0069524866497120566,  0.21288810409948347,      0.0017810057298691725,
        0.088760951809475269,   0.0055957188940032095,   -0.31937177676259115,     -0.0087539416335935556,
        0.34748800245527145,    0.0075378299617709235,   -0.25116537379803394,     -0.0047285674679876204,
        0.13970502851683486,    0.0023672533925155220,   -0.063649401080083698,    -0.00098993213448982727,
        0.024597838934816905,   0.00035553235917057483,  -0.0082485030307578155,   -0.00011176184313622549,
        0.0024390574829093264,  0.000031180384864488629, -0.00064373524734389861,  -7.8036008952377965e-6,
        0.00015310015145922058, 1.7670804180220134e-6,   -0.000033066844379476900, -3.6460909134279425e-7,
        6.5276969021754105e-6,  6.8957843666189918e-8,   -1.1842811187642386e-6,   -1.2015133285307312e-8,
        1.9839339947648331e-7,  1.9372045971100854e-9,   -3.0815418032523593e-8,   -2.9013806338735810e-10,
        4.4540904298173700e-9,  4.0505136697916078e-11,  -6.0104912807134771e-10,  -5.2873323696828491e-12,
        7.5943206779351725e-11, 6.4679566322060472e-13,  -9.0081200925539902e-12,  -7.4396949275292252e-14,
        1.0057423059167244e-12, 8.1701187638005194e-15,  -1.0611736208855373e-13,  -8.9597492970451533e-16,
        1.1421575296031385e-14};
};

}  // namespace lbcrypto

#endif
