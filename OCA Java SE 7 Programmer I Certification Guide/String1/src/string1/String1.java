package string1;

public class String1 {
	public static void main(String args[]){
		String morning1 = "Morning";
		System.out.println("Morning" == morning1); //true
		
		String morning2 = new String("Morning");
		System.out.println("Morning" == morning2); //false
		
		StringBuilder sd1 = new StringBuilder("String Builder");
		String str5 = new String(sd1);
		System.out.println(str5);
		
		StringBuffer sb2 = new StringBuffer("String Buffer");
		String str6 = new String(sb2);
		System.out.println(str6);
	}
}
