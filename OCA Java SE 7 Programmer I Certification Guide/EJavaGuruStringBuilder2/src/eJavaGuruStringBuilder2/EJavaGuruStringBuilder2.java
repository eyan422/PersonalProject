package eJavaGuruStringBuilder2;

class EJavaGuruStringBuilder2 {
	public static void main(String args[]) {
		StringBuilder sb1 = new StringBuilder("123456");
		sb1.subSequence(2, 4);
		sb1.deleteCharAt(3);
		sb1.reverse();
		System.out.println(sb1);
	}
}
