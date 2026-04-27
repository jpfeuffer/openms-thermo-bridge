#include <openms_thermo_bridge/cv_mapping.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openms::thermo_bridge
{
namespace
{

// ================================================================
//  Utility: case-insensitive string helpers
// ================================================================

std::string to_upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool starts_with_ci(const std::string& haystack, const std::string& prefix)
{
    if (prefix.size() > haystack.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
    {
        if (std::toupper(static_cast<unsigned char>(haystack[i])) !=
            std::toupper(static_cast<unsigned char>(prefix[i])))
            return false;
    }
    return true;
}

bool contains_ci(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    auto upper_haystack = to_upper(haystack);
    auto upper_needle = to_upper(needle);
    return upper_haystack.find(upper_needle) != std::string::npos;
}

// ================================================================
//  Instrument model mapping table
//
//  Ported from ThermoRawFileParser Writer/OntologyMapping.cs
//  GetInstrumentModel().  Sorted longest-key-first so that
//  "LTQ Orbitrap Discovery" matches before "LTQ Orbitrap".
// ================================================================

struct InstrumentMapping
{
    const char* prefix;     // prefix to match (case-insensitive)
    const char* accession;
    const char* name;
};

// Ordered from most specific (longest) to least specific so that
// the first match wins.
const InstrumentMapping instrument_table[] = {
    // MALDI variants (long prefixes first)
    {"MALDI LTQ ORBITRAP DISCOVERY", "MS:1003497", "MALDI LTQ Orbitrap Discovery"},
    {"MALDI LTQ ORBITRAP XL",       "MS:1003496", "MALDI LTQ Orbitrap XL"},
    {"MALDI LTQ ORBITRAP",          "MS:1000643", "MALDI LTQ Orbitrap"},
    {"MALDI LTQ XL",                "MS:1000642", "MALDI LTQ XL"},

    // LTQ Orbitrap variants (long prefixes first)
    {"LTQ ORBITRAP VELOS PRO",      "MS:1003096", "LTQ Orbitrap Velos Pro"},
    {"LTQ ORBITRAP VELOS ETD",      "MS:1003499", "LTQ Orbitrap Velos ETD"},
    {"LTQ ORBITRAP VELOS",          "MS:1001742", "LTQ Orbitrap Velos"},
    {"LTQ ORBITRAP ELITE",          "MS:1001910", "LTQ Orbitrap Elite"},
    {"LTQ ORBITRAP DISCOVERY",      "MS:1000555", "LTQ Orbitrap Discovery"},
    {"LTQ ORBITRAP CLASSIC",        "MS:1002835", "LTQ Orbitrap Classic"},
    {"LTQ ORBITRAP XL ETD",         "MS:1000639", "LTQ Orbitrap XL ETD"},
    {"LTQ ORBITRAP XL",             "MS:1000556", "LTQ Orbitrap XL"},
    {"LTQ ORBITRAP",                "MS:1000449", "LTQ Orbitrap"},

    // LTQ FT
    {"LTQ FT ULTRA",                "MS:1000557", "LTQ FT Ultra"},
    {"LTQ FT",                      "MS:1000448", "LTQ FT"},

    // LTQ variants
    {"LTQ VELOS ETD",               "MS:1000856", "LTQ Velos ETD"},
    {"VELOS PLUS",                   "MS:1003495", "Velos Plus"},
    {"LTQ VELOS",                    "MS:1000855", "LTQ Velos"},
    {"LTQ XL ETD",                   "MS:1000638", "LTQ XL ETD"},
    {"LTQ XL",                       "MS:1000854", "LTQ XL"},
    {"LTQ",                          "MS:1000447", "LTQ"},
    {"LXQ",                          "MS:1000450", "LXQ"},

    // Modern Orbitrap series (long prefixes first)
    {"ORBITRAP FUSION LUMOS",        "MS:1002732", "Orbitrap Fusion Lumos"},
    {"ORBITRAP FUSION ETD",          "MS:1002417", "Orbitrap Fusion ETD"},
    {"ORBITRAP FUSION",              "MS:1002416", "Orbitrap Fusion"},
    {"ORBITRAP ECLIPSE",             "MS:1003029", "Orbitrap Eclipse"},
    {"ORBITRAP ASCEND",              "MS:1003356", "Orbitrap Ascend"},
    {"ORBITRAP ID-X",                "MS:1003112", "Orbitrap ID-X"},
    {"ORBITRAP EXPLORIS 120",        "MS:1003095", "Orbitrap Exploris 120"},
    {"ORBITRAP EXPLORIS 240",        "MS:1003094", "Orbitrap Exploris 240"},
    {"ORBITRAP EXPLORIS 480",        "MS:1003028", "Orbitrap Exploris 480"},
    {"ORBITRAP ASTRAL",              "MS:1003378", "Orbitrap Astral"},

    // Exactive series
    {"Q EXACTIVE HF-X",             "MS:1002877", "Q Exactive HF-X"},
    {"Q EXACTIVE HF",               "MS:1002523", "Q Exactive HF"},
    {"Q EXACTIVE PLUS",             "MS:1002634", "Q Exactive Plus"},
    {"Q EXACTIVE",                   "MS:1001911", "Q Exactive"},
    {"EXACTIVE PLUS",                "MS:1002526", "Exactive Plus"},
    {"EXACTIVE",                     "MS:1000649", "Exactive"},

    // TSQ and other instruments
    {"TSQ QUANTUM ACCESS MAX",       "MS:1003498", "TSQ Quantum Access MAX"},
    {"TSQ QUANTUM XLS",              "MS:1003502", "TSQ Quantum XLS"},
    {"TSQ 8000",                     "MS:1003503", "TSQ 8000 Evo"},
    {"ISQ LT",                       "MS:1003500", "ISQ LT"},
    {"ITQ",                          "MS:1003501", "ITQ"},
    {"DELTAPLUS",                    "MS:1003504", "DeltaPlus Advantage"},
    {"THERMOQUEST VOYAGER",          "MS:1003554", "ThermoQuest Voyager"},
};

constexpr std::size_t instrument_table_size =
    sizeof(instrument_table) / sizeof(instrument_table[0]);

// ================================================================
//  Mass analyzer mapping
// ================================================================

struct StringMapping
{
    const char* key;
    const char* accession;
    const char* name;
};

const StringMapping analyzer_table[] = {
    {"MassAnalyzerITMS",    "MS:1000264", "ion trap"},
    {"MassAnalyzerTQMS",    "MS:1000081", "quadrupole"},
    {"MassAnalyzerSQMS",    "MS:1000081", "quadrupole"},
    {"MassAnalyzerTOFMS",   "MS:1000084", "time-of-flight"},
    {"MassAnalyzerSector",  "MS:1000080", "magnetic sector"},
    {"MassAnalyzerASTMS",   "MS:1003379", "asymmetric track lossless time-of-flight analyzer"},
    // FTMS is handled specially — see cv_mass_analyzer()
};

constexpr std::size_t analyzer_table_size =
    sizeof(analyzer_table) / sizeof(analyzer_table[0]);

// ================================================================
//  Ionization mode mapping
// ================================================================

const StringMapping ionization_table[] = {
    {"ElectroSpray",                                "MS:1000073", "electrospray ionization"},
    {"NanoSpray",                                   "MS:1000398", "nanoelectrospray"},
    {"CardNanoSprayIonization",                     "MS:1000398", "nanoelectrospray"},
    {"AtmosphericPressureChemicalIonization",       "MS:1000070", "atmospheric pressure chemical ionization"},
    {"ChemicalIonization",                          "MS:1000071", "chemical ionization"},
    {"MatrixAssistedLaserDesorptionIonization",     "MS:1000239", "atmospheric pressure matrix-assisted laser desorption ionization"},
    {"PaperSprayIonization",                        "MS:1003235", "paper spray ionization"},
    {"ElectronImpact",                              "MS:1000389", "electron ionization"},
    {"FastAtomBombardment",                         "MS:1000074", "fast atom bombardment ionization"},
    {"ThermoSpray",                                 "MS:1000069", "thermospray ionization"},
    {"FieldDesorption",                             "MS:1000257", "field desorption"},
    {"GlowDischarge",                               "MS:1000259", "glow discharge ionization"},
};

constexpr std::size_t ionization_table_size =
    sizeof(ionization_table) / sizeof(ionization_table[0]);

// ================================================================
//  Activation / dissociation type mapping
// ================================================================

const StringMapping activation_table[] = {
    {"CollisionInducedDissociation",           "MS:1000133", "collision-induced dissociation"},
    {"HigherEnergyCollisionalDissociation",    "MS:1000422", "beam-type collision-induced dissociation"},
    {"ElectronTransferDissociation",           "MS:1000598", "electron transfer dissociation"},
    {"MultiPhotonDissociation",                "MS:1000262", "infrared multiphoton dissociation"},
    {"ElectronCaptureDissociation",            "MS:1000250", "electron capture dissociation"},
    {"PQD",                                    "MS:1000599", "pulsed-q dissociation"},
    {"ProtonTransferReaction",                 "MS:1003249", "proton transfer reaction"},
    {"NegativeProtonTransferReaction",         "MS:1003249", "proton transfer reaction"},
    {"NegativeElectronTransferDissociation",   "MS:1003247", "negative electron transfer dissociation"},
    {"UltraVioletPhotoDissociation",           "MS:1003246", "ultraviolet photodissociation"},
};

constexpr std::size_t activation_table_size =
    sizeof(activation_table) / sizeof(activation_table[0]);

// ================================================================
//  Detector type table (by instrument prefix)
// ================================================================

struct DetectorMapping
{
    const char* instrument_prefix;
    const char* accession;
    const char* name;
};

// Instruments with inductive detectors (MS:1000624)
const char* inductive_detector_instruments[] = {
    "LTQ FT",
    "LTQ ORBITRAP",
    "MALDI LTQ ORBITRAP",
    "ORBITRAP FUSION",
    "ORBITRAP ECLIPSE",
    "ORBITRAP ASCEND",
    "ORBITRAP EXPLORIS",
    "Q EXACTIVE",
    "EXACTIVE",
    "ORBITRAP ID-X",
    "ORBITRAP ASTRAL",
};

// Instruments with electron multiplier detectors (MS:1000253)
const char* electron_multiplier_instruments[] = {
    "LTQ",
    "LXQ",
    "TSQ",
    "ISQ",
    "ITQ",
    "MALDI LTQ XL",
    "THERMOQUEST VOYAGER",
    "ORBITRAP ID-X",   // also has inductive
};

} // anonymous namespace

// ================================================================
//  cv_instrument_model
// ================================================================

CVTerm cv_instrument_model(const std::string& model)
{
    std::string upper = to_upper(model);

    // Try longest-prefix match.
    for (std::size_t i = 0; i < instrument_table_size; ++i)
    {
        if (starts_with_ci(upper, instrument_table[i].prefix))
        {
            return {instrument_table[i].accession, "MS",
                    instrument_table[i].name, ""};
        }
    }

    // Fallback: generic Thermo Fisher Scientific instrument model.
    return {"MS:1000483", "MS", "Thermo Fisher Scientific instrument model", ""};
}

// ================================================================
//  cv_mass_analyzer
// ================================================================

CVTerm cv_mass_analyzer(const std::string& analyzer_type,
                        const std::string& instrument_model)
{
    // FTMS is special: could be FT-ICR or Orbitrap.
    if (analyzer_type == "MassAnalyzerFTMS" || analyzer_type == "FTMS")
    {
        // If the instrument model contains "Orbitrap" or "Exactive" or
        // "Exploris" or "Astral", it uses an Orbitrap analyzer.
        if (contains_ci(instrument_model, "Orbitrap") ||
            contains_ci(instrument_model, "Exactive") ||
            contains_ci(instrument_model, "Exploris") ||
            contains_ci(instrument_model, "Astral"))
        {
            return {"MS:1000484", "MS", "orbitrap", ""};
        }
        return {"MS:1000079", "MS", "fourier transform ion cyclotron resonance mass spectrometer", ""};
    }

    // Look up in the static table.
    for (std::size_t i = 0; i < analyzer_table_size; ++i)
    {
        if (analyzer_type == analyzer_table[i].key)
        {
            return {analyzer_table[i].accession, "MS",
                    analyzer_table[i].name, ""};
        }
    }

    // Fallback: generic mass analyzer type.
    return {"MS:1000443", "MS", "mass analyzer type", ""};
}

// ================================================================
//  cv_ionization_mode
// ================================================================

CVTerm cv_ionization_mode(const std::string& ionization_type)
{
    for (std::size_t i = 0; i < ionization_table_size; ++i)
    {
        if (ionization_type == ionization_table[i].key)
        {
            return {ionization_table[i].accession, "MS",
                    ionization_table[i].name, ""};
        }
    }

    return {"MS:1000008", "MS", "ionization type", ""};
}

// ================================================================
//  cv_activation_type
// ================================================================

CVTerm cv_activation_type(const std::string& activation_type)
{
    for (std::size_t i = 0; i < activation_table_size; ++i)
    {
        if (activation_type == activation_table[i].key)
        {
            return {activation_table[i].accession, "MS",
                    activation_table[i].name, ""};
        }
    }

    return {"MS:1000044", "MS", "dissociation method", ""};
}

// ================================================================
//  cv_detector_types
// ================================================================

std::vector<CVTerm> cv_detector_types(const std::string& instrument_model)
{
    std::string upper = to_upper(instrument_model);
    std::vector<CVTerm> result;

    // Check for inductive detector (Orbitrap / FT-ICR analyzers).
    for (const auto* prefix : inductive_detector_instruments)
    {
        if (starts_with_ci(upper, prefix))
        {
            result.push_back({"MS:1000624", "MS", "inductive detector", ""});
            break;
        }
    }

    // Check for electron multiplier (ion trap / quad instruments).
    for (const auto* prefix : electron_multiplier_instruments)
    {
        if (starts_with_ci(upper, prefix))
        {
            // Don't duplicate if already found via Orbitrap ID-X
            bool already_has_em = false;
            for (const auto& t : result)
            {
                if (t.accession == "MS:1000253") { already_has_em = true; break; }
            }
            if (!already_has_em)
                result.push_back({"MS:1000253", "MS", "electron multiplier", ""});
            break;
        }
    }

    // Orbitrap Astral has a special second detector.
    if (starts_with_ci(upper, "ORBITRAP ASTRAL"))
    {
        result.push_back({"MS:1000108", "MS",
                          "conversion dynode electron multiplier", ""});
    }

    // DeltaPlus IRMS has a Faraday cup.
    if (starts_with_ci(upper, "DELTAPLUS") || starts_with_ci(upper, "DELTA PLUS"))
    {
        result.push_back({"MS:1000112", "MS", "faraday cup", ""});
    }

    // If we could not determine any detector, return empty.
    return result;
}

} // namespace openms::thermo_bridge
