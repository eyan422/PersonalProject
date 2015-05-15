package ejava3;

class EJavaBase {
	void myMethod() throws ExceptionInInitializerError {
		System.out.println("Base");
	}
}

class EJavaDerived extends EJavaBase {
	void myMethod() throws RuntimeException {
		System.out.println("Derived");
	}
}

class EJava3 {
	public static void main(String args[]) {
		EJavaBase obj = new EJavaDerived();
		obj.myMethod();
	}
}