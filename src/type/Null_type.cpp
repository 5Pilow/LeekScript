#include "Null_type.hpp"
#include "Type.hpp"
#include "../colors.h"
#include "Any_type.hpp"

namespace ls {

bool Null_type::operator == (const Base_type* type) const {
	return dynamic_cast<const Null_type*>(type);
}
bool Null_type::castable(const Base_type* type) const {
	if (dynamic_cast<const Null_type*>(type)
		or dynamic_cast<const Any_type*>(type)) {
		return true;
	}
	return false;
}
llvm::Type* Null_type::llvm() const {
	return Any_type::get_any_type();
}
std::string Null_type::clazz() const {
	return "Null";
}
std::ostream& Null_type::print(std::ostream& os) const {
	os << BLUE_BOLD << "null" << END_COLOR;
	return os;
}

}