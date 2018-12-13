#ifndef INTEGER_TYPE_HPP
#define INTEGER_TYPE_HPP

#include "Number_type.hpp"

namespace ls {

class Integer_type : public Number_type {
public:
	Integer_type() {}
	virtual const std::string getName() const { return "int"; }
	virtual const std::string getJsonName() const { return "number"; }
	virtual Type element() const override;
	virtual Type iterator() const override;
	virtual bool operator == (const Base_type*) const;
	virtual int distance(const Base_type* type) const override;
	virtual std::ostream& print(std::ostream&) const override;
	virtual llvm::Type* llvm() const override;
};

}

#endif