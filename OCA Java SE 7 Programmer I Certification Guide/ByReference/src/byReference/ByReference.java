package byReference;

public class ByReference {
	public static void main(String args[]){
		StringBuilder myArr[] = {
				new StringBuilder("Java"),
				new StringBuilder("Loop")
		};
		
		for (StringBuilder val : myArr)
			System.out.println(val);
		/*
		for (StringBuilder val : myArr)
			val.append("Oracle");
		*/
		
		for (StringBuilder val : myArr)
		{
			val = new StringBuilder("Oracle");
			System.out.println(val);
		}
		
		for (StringBuilder val : myArr)
			System.out.println(val);
	}
}
