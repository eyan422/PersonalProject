package inheritancereference;

class Base {
	String var = "EJava";
	
	void printVar() {
		System.out.println(var);
	}
}

class Derived extends Base {
	String var = "Guru";
	
	void printVar() {
		System.out.println(var);
	}
}

class InheritanceReference {
	public static void main(String[] args) {
		Base base = new Base();
		Base derived = new Derived();
		
		System.out.println(base.var);
		System.out.println(derived.var);
		
		base.printVar();
		derived.printVar();
	}
}
