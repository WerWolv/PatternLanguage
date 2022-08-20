#pragma once

namespace pl {
    class PatternLanguage;
}

namespace pl::lib::libstd {

    void registerPragmas(pl::PatternLanguage &runtime);

    namespace libstd    { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace mem       { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace math      { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace string    { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace file      { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace time      { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace core      { void registerFunctions(pl::PatternLanguage &runtime); }

    inline void registerFunctions(pl::PatternLanguage &runtime) {
        registerPragmas(runtime);

        libstd::registerFunctions(runtime);
        mem::registerFunctions(runtime);
        math::registerFunctions(runtime);
        string::registerFunctions(runtime);
        file::registerFunctions(runtime);
        time::registerFunctions(runtime);
        core::registerFunctions(runtime);
    }

}