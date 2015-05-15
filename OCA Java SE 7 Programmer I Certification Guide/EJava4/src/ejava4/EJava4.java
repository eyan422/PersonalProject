package ejava4;

class EJava4 {
	void foo() {
		try {
			String s = null;
			System.out.println("1");
			
			try {
				System.out.println(s.length());
			}
			catch (NullPointerException e) {
				System.out.println("inner");
			}
			
			System.out.println("2");
		}
		catch (NullPointerException e) {
			System.out.println("outer");
		}
	}
	
	public static void main(String args[]) {
		EJava4 obj = new EJava4();
		obj.foo();
	}
}