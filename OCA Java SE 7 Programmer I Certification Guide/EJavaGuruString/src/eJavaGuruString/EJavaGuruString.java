package eJavaGuruString;

class EJavaGuruString {
	public static void main(String args[]) {
		String ejg1 = new String("E Java");
		String ejg2 = new String("E Java");
		String ejg3 = "E Java";
		String ejg4 = "E Java";
		
		/*
		do
			System.out.println(ejg1.equals(ejg2));
		while (ejg3 == ejg4);
		*/
		System.out.println(ejg1.equals(ejg2));
		System.out.println(ejg1 == ejg2);
		
		System.out.println(ejg3.equals(ejg4));
		System.out.println(ejg3 == ejg4);
		
		System.out.println(ejg3);
	}
}
