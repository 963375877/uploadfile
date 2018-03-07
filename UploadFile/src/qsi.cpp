#include "var.h"

std::string url_decode( const std::string &eString ) {
	std::string ret;
	char ch;
	int i, j;
	for ( i = 0; i < eString.length(); i++ ) {
		if ( int( eString[i] ) == 37 ) {
			sscanf( eString.substr(i+1,2).c_str(), "%x", &j );
			ch = static_cast<char>(j);
			ret += ch;
			i = i+2;
		} else {
			ret += eString[i];
		}
	}
	return (ret);
}


bool qsi( const std::string &str, var &index, int &i, int level = 0 ) {
	if( str == "" ) return false;

    int j = 0;
    std::string in = "";
    int ind = 0;
    std::string name = "";
    bool start = false;
    while( true ) {


        if( i >= str.length() || ( str[i] == '&' && i != 0 ) ) {

        	if( name != "" ) {
        		i++;
        		index[name] = url_decode( in );
        	
        		name = "";
        		in = "";
        	} else {
	            if( level != 0 ) {
	                index = url_decode( in );
	            	return true;		            	
	            } else {
	            	if( in != "" )
	 					index[in];
	 			}

        	}

        	if( i >= str.length() ) return true; 

        }

        switch( str[i] ) {
         
            case '?' : {
            	in = "";
                i++;
            } break;
            case '&' : {
            	in = "";
                i++;
            }break;
            case '=' : {
            	name = in;
            	
            	if( str[i+1] == '&' ) {
            		i++;
            		in = "";
            	} else {
            		i++;
            		in = str[i++];
            	}
            } break;
            case ']' : {
                i++;
                
                qsi( str, index[in], i, level + 1 );
                return true;
            } break;
            case '[' : {
    
                if( in != "" ) {
         
                    qsi( str, index[in], i, level + 1 );

                    if( str[i] == '&' ) {
                        i++;

                        qsi( str, index, i, level + 1 );
                    }
                 
                    return true;
                }

                in = "";
                if( str[i+1] == ']' ) {
                    i++;
                    in += std::to_string( index.size() );
                } else {
                    if( str[i+1] == '=' ) {
                        i+=2;
                    } else {
                        i++;
                        in += str[i++];                         
                    }

                }

            } break;
            default : {

                in += str[i++];  
            } break;             
        }
    }

}

char * get_query_string(char *querystring, char *name)
{
	if( querystring == "") 
    	 return NULL;
   	 
    var arr;
    int i = 0;
    qsi( querystring, arr, i );

	return strdup(arr[name].c_str()); 
}


/*
int main( int argc, char** argv  ) {

	std::string qs = argv[1];
	if( qs == "") {
		return 0;
	}
	
	var arr;
	int i = 0;
	qsi( qs, arr, i );

	print_r( arr );
}
*/
