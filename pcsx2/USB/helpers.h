#ifndef HELPERS_H
#define HELPERS_H

struct SelectKey {
	template <typename F, typename S>
	F operator()(const std::pair<const F, S> &x) const { return x.first; }
};

#endif
