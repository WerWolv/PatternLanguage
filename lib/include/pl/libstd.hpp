#pragma once

namespace pl {
    class PatternLanguage;
}

namespace pl::libstd {

    namespace libstd    { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace mem       { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace math      { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace string    { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace file      { void registerFunctions(pl::PatternLanguage &runtime); }
    namespace time      { void registerFunctions(pl::PatternLanguage &runtime); }

    void registerFunctions(pl::PatternLanguage &runtime) {
        libstd::registerFunctions(runtime);
        mem::registerFunctions(runtime);
        math::registerFunctions(runtime);
        string::registerFunctions(runtime);
        file::registerFunctions(runtime);
        time::registerFunctions(runtime);
    }

}