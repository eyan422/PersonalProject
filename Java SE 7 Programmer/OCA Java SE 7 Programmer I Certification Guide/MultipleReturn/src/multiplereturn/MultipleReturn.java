package multiplereturn;

class MultipleReturn {
	@SuppressWarnings("finally")
	int getInt() {
		try {
			String[] students = {"Harry", "Paul"};
			System.out.println(students[5]);
		}
		catch (Exception e) {
			return 10;
		}
		finally {
			return 20;
		}
	}
	
	public static void main(String args[]) {
		MultipleReturn var = new MultipleReturn();
		System.out.println(var.getInt());
	}
}