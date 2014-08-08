create or replace
PROCEDURE UFIDS_TEST_DBLINK AS 
  
v_stat 		VARCHAR2(4000);
V_Cur 		Integer;
v_ret     NUMBER;

PclMaxId 	ufids_current_arrivals.Appn_Row_Id%Type;
PclMaxDate 	ufids_current_arrivals.Row_date_Time%Type;
Query_Str Varchar2(2000);

v_db_link      VARCHAR2(256) := 'UFIS_YYZ51';   
v_source_tbl   VARCHAR2(256) := 'ARR_TABLE'||'@'||v_db_link;
v_dest_tbl     VARCHAR2(256) := 'Ufids_Current_Arrivals_x'; 

/* start with highest Appn_Row_Id (insert record) or Row_Date_Time (update record) in local table Ufids_Current_Arrivals_x */
Cursor Tabcur IS 
			Select Max(Appn_Row_Id),Max(Row_Date_Time) From Ufids_Current_Arrivals_x; 

BEGIN
    OPEN TabCur;
    Begin
        Fetch Tabcur Into PclMaxId,PclMaxDate;
		Dbms_Output.Put_Line( 'Max ID:' || Pclmaxid); 
        Dbms_Output.Put_Line( 'Max Date:' || to_char(Pclmaxdate, 'MM/DD/YYYY HH24:MI:SS'));
		    
		Query_Str := 'TRUNCATE TABLE ' || v_dest_tbl; 
		Dbms_Output.Put_Line(Query_Str);
		Execute Immediate Query_Str; commit;
		
		Dbms_Output.Put_Line( 'Refill ufids_current_arrivals:');
		
		If Pclmaxid Is Null
		Then
			Dbms_Output.Put_Line( 'Pclmaxid is null');
			If Pclmaxdate Is Null
			Then
				Dbms_Output.Put_Line( 'Pclmaxdate is null');				
				Query_Str := 'Insert Into ' || v_dest_tbl || '(select * from ' || v_source_tbl || ')';
				Dbms_Output.Put_Line(Query_Str);				
				Execute Immediate Query_Str; commit;				
			Else
				Dbms_Output.Put_Line( 'Pclmaxdate is not null');
		
				Query_Str := 'Insert Into ' || v_dest_tbl || '(select * from ' || v_source_tbl || ' WHERE to_char(Row_date_Time,YYYYMMDDHH24MISS) >= ' || to_char(Pclmaxdate,'YYYYMMDDHH24MISS') || ')';
				Dbms_Output.Put_Line(Query_Str);				
				Execute Immediate Query_Str; commit;
			end if; 
		Else
			Dbms_Output.Put_Line( 'Pclmaxid is not null');
			If Pclmaxdate Is Null
			Then
				Dbms_Output.Put_Line( 'Pclmaxdate is null');
				
				Query_Str := 'Insert Into ' || v_dest_tbl || '(select * from ' || v_source_tbl || ' WHERE Appn_Row_Id >= '|| PclMaxId || ')';				
				Dbms_Output.Put_Line(Query_Str);				
				Execute Immediate Query_Str; commit;		
			Else
				Dbms_Output.Put_Line( 'Pclmaxdate is not null');
				
				Query_Str := 'Insert Into ' || v_dest_tbl || '(SELECT * from ' || v_source_tbl || ' WHERE Appn_Row_Id >= '|| PclMaxId || ' OR to_char(Row_date_Time,YYYYMMDDHH24MISS) >= ' || to_char(Pclmaxdate,'YYYYMMDDHH24MISS') || ')';
				Dbms_Output.Put_Line(Query_Str);				
				Execute Immediate Query_Str; commit;					
			End if; 
			commit;
		End If;    
    End;
    CLOSE TabCur;
    ROLLBACK;
END;