package appendstringbuilder;

public class AppendStringBuilder {
	public static void main(String args[]) {
		StringBuilder sb1 = new StringBuilder();
		sb1.append(true);
		sb1.append(10);
		sb1.append('a');
		sb1.append(20.99);
		sb1.append("Hi");
		System.out.println(sb1);
		
		StringBuilder sb2 = new StringBuilder();
		char[] name = {'J', 'a', 'v', 'a', '7'};
		sb2.append(name, 1, 3);
		System.out.println(sb2);
	}
}
