package eJavaGuruStringBuilder;

class EJavaGuruStringBuilder {
	public static void main(String args[]) {
		StringBuilder ejg = new StringBuilder(10 + 2 + "SUN" + 4 + 5);
		
		System.out.println(ejg);
		//System.out.println(ejg.delete(3, 6));
		
		ejg.append(ejg.delete(3, 6));
		System.out.println(ejg);
	}
}

/*
12SUN45
12S512S5
*/