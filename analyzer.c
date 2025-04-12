#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_c.c"

/* 1. 함수 개수 추출 함수 */

int count_functions(json_value root) {

    // 함수 개수를 저장할 변수 초기화 
    int count = 0;
    
    // json 루트 객체에서 "ext"에 해당하는 값 추출 
    json_value ext = json_get(root, "ext");
    
    // ext 배열의 길이 계산 
    int ext_len = json_len(ext);
    
    // ext 배열 순회하며 요소 추출 
    for (int i = 0; i < ext_len; i++) {
        json_value obj = json_get(ext, i);
        
        // 현재 객체에서 "_nodetype"에 해당하는 값 추출 
        json_value nodetype = json_get(obj, "_nodetype");
        
        // case 1 : FuncDef 체크
        if (strcmp(json_get_string(nodetype), "FuncDef") == 0) {
            count++;
            continue;
        }

        // case 2 : FuncDecl 체크
        // 현재 객체에서 "type" 필드 추출 
        json_value type = json_get(obj, "type");
        // "type" 객체에서 "_nodetype" 필드 추출 
        json_value decl_type = json_get(type, "_nodetype");
        if (strcmp(json_get_string(decl_type), "FuncDecl") == 0) {
                count++;
        }
    }
    return count;
}

/* 2. 리턴 타입 추출 함수 */

void resolve_type(json_value type, char* buffer, int depth) {

    // type 객체에서 "_nodetype" 필드 추출 
    json_value nodetype = json_get(type, "_nodetype");
    
    // "_nodetype" 값을 문자열로 변환 
    const char* type_str = json_get_string(nodetype);

    // 1) PtrDecl : 포인터 타입 
    if (strcmp(type_str, "PtrDecl") == 0) {
        strcat(buffer, "*");
        resolve_type(json_get(type, "type"), buffer, depth+1);
    }

    // 2) ArrayDecl : 배열 타입 
    else if (strcmp(type_str, "ArrayDecl") == 0) {
        strcat(buffer, "[]");
        resolve_type(json_get(type, "type"), buffer, depth+1);
    }

    // 3) TypeDecl : 내부 type 필드로 연결 
    else if (strcmp(type_str, "TypeDecl") == 0) {
        resolve_type(json_get(type, "type"), buffer, depth + 1);
    }

    // 3) IdentifierType : 기본 타입 
    else if (strcmp(type_str, "IdentifierType") == 0) {
            
        // names 배열 추출 
        json_value names = json_get(type, "names");
        
        // names 배열 길이 계산
        int name_count = json_len(names);
        
        // 배열 순회하며 버퍼에 타입 이름 추가 
        for (int i = 0; i < name_count; i++) {
            json_value name = json_get(names, i);
            strcat(buffer, json_get_string(name)); 
            // 복합 타입일 경우 공백 추가 
            if (i != name_count-1) strcat(buffer, " ");
        }
    }
}

/* 3. 파라미터 추출 함수 */

void get_parameters(json_value params, char* result) {

    // 파라미터 개수 
    int param_count = json_len(params);

    // 모든 파라미터 순회 
    for (int i = 0; i < param_count; i++) {
        json_value param = json_get(params, i);

        // 파라미터 타입을 저장할 버퍼 
        char type_buf[256] = {0};

        // 파라미터 이름을 저장할 버퍼 
        char name_buf[128] = {0};

        // 타입 추출
        json_value type = json_get(param, "type");
        resolve_type(type, type_buf, 0);

        // 이름 추출
        json_value name = json_get(param, "name");
        if (name.type == JSON_STRING) {
            strncpy(name_buf, json_get_string(name), sizeof(name_buf)-1);
        } else {
            snprintf(name_buf, sizeof(name_buf), "param%d", i);  // 이름 없으면 기본 이름 사용
        }
        // 버퍼가 넘치지 않을 경우에만 결과 문자열에 추가 
        if (strlen(result) + strlen(type_buf) + strlen(name_buf) < 510) {
            // 형식: "타입 이름" 추가
            sprintf(result + strlen(result), "%s %s", type_buf, name_buf);
             // 마지막 파라미터가 아니면 쉼표 추가
            if (i != param_count-1) strcat(result, ", ");
        }
    }
}

/* 4. IF 조건 개수 추출 함수 */

// 키 존재 여부를 확인하는 json_has_key() 함수 추가
bool json_has_key(json_value obj, const char* key) {
    if (obj.type != JSON_OBJECT) return false;
    json_object* jsobj = (json_object*)obj.value;
    for (int i = 0; i <= jsobj->last_index; i++) {
        if (strcmp(jsobj->keys[i], key) == 0) return true;
    }
    return false;
}

void count_if_recursive(json_value node, int* count) {
    // 현재 노드가 If인 경우
    if (json_has_key(node, "_nodetype")) {
        json_value nodetype = json_get(node, "_nodetype");
        if (nodetype.type == JSON_STRING && strcmp(json_get_string(nodetype), "If") == 0) {
            (*count)++;
        }
    }

    // 객체인 경우: 모든 키의 값에 대해 재귀 탐색
    if (node.type == JSON_OBJECT) {
        json_object* obj = (json_object*)node.value;
        for (int i = 0; i <= obj->last_index; i++) {
            json_value child = obj->values[i];
            count_if_recursive(child, count);
        }
    }

    // 배열인 경우: 각 요소에 대해 재귀 탐색
    if (node.type == JSON_ARRAY) {
        int len = json_len(node);
        for (int i = 0; i < len; i++) {
            json_value child = json_get(node, i);
            count_if_recursive(child, count);
        }
    }
}

// 주어진 함수에서 if 조건문의 개수를 계산하는 함수
int count_if_conditions(json_value func) {
    int count = 0;
    count_if_recursive(func, &count);
    return count;
}


int main(void) {   

    /* JSON 파일 열기 */ 
    
FILE *fp = fopen("ast.json", "r");
if (fp == NULL) {
    printf("파일을 열 수 없습니다.\n");
    return 1;
}

/* 파일 크기 측정 및 메모리 할당 */ 

fseek(fp, 0, SEEK_END);     // 파일 끝으로 이동 
long file_size = ftell(fp); // 파일 크기 가져오기 
rewind(fp);                 // 파일 포인터 다시 처음으로 

char *file_buff = (char *)malloc(file_size + 1);
if (!file_buff) {
    fclose(fp);
    return 1;
}

/* 파일 읽기 */  

fread(file_buff, 1, file_size, fp); 
file_buff[file_size] = '\0';

/* 파일 닫기 */

fclose(fp);

/* JSON 파싱 시작 */ 

json_value json = json_create(file_buff);
if (json.type != JSON_OBJECT) {
    printf("잘못된 JSON 형식\n");
    free(file_buff);
    return 1;
}

/* 1. 함수 개수 출력 */

int total_func = count_functions(json);
printf("Number of Functions : %d\n\n", total_func);

/* 2. 함수 정보 추출 */

/* 2. 함수 정보 추출 */
json_value ext = json_get(json, "ext");  // 함수 목록은 "ext" 배열 안에 있음
if (ext.type == JSON_ARRAY) {
    int ext_len = json_len(ext);  // 배열 길이 구하기

    // 배열 내 모든 항목(함수 또는 선언)을 순회
    for (int i = 0; i < ext_len; i++) {
        json_value func = json_get(ext, i);
        if (func.type != JSON_OBJECT) continue;  // 객체가 아니면 skip

        json_value nodetype = json_get(func, "_nodetype");
        const char *nodetype_str = NULL;
        if (nodetype.type == JSON_STRING) {
            nodetype_str = json_get_string(nodetype);
        }

        char func_name[128] = "익명";  // 기본 함수 이름
        char return_type[256] = "";   // 리턴 타입 초기화

        json_value type_node = {0};  // 리턴 타입 추출용 노드

        // 함수 정의 (FuncDef)
        if (strcmp(nodetype_str, "FuncDef") == 0) {
            json_value decl = json_get(func, "decl");  // decl 안에 함수 정의 있음
            json_value name = json_get(decl, "name");  // 이름 추출
            if (name.type == JSON_STRING) {
                snprintf(func_name, sizeof(func_name), "%s", json_get_string(name));
            }
            type_node = json_get(decl, "type");  // 리턴 타입으로 내려가는 경로
        } 
        // 함수 선언 (Decl)
        else {
            json_value name = json_get(func, "name");
            if (name.type == JSON_STRING) {
                snprintf(func_name, sizeof(func_name), "%s", json_get_string(name));
            }
            type_node = json_get(func, "type");
        }

        // 리턴 타입 처리
        if (type_node.type != JSON_UNDEFINED) {
            json_value nodetype = json_get(type_node, "_nodetype");
            const char *ntype = json_get_string(nodetype);

            if (strcmp(ntype, "FuncDecl") == 0) {
                type_node = json_get(type_node, "type");  // 실제 리턴 타입으로 내려가기
            }

            resolve_type(type_node, return_type, 0);  // 타입 문자열로 변환
        } else {
            strcpy(return_type, "void");  // 타입이 없으면 void 처리
        }

        /* 파라미터 처리 */
        char params[512] = "";
        int has_params = 0;

        json_value args = {0};  // args 정보 저장용

        if (strcmp(nodetype_str, "FuncDef") == 0) {
            json_value decl = json_get(func, "decl");
            json_value decl_type = json_get(decl, "type");

            json_value decl_type_tag = json_get(decl_type, "_nodetype");
            if (strcmp(json_get_string(decl_type_tag), "FuncDecl") == 0) {
                args = json_get(decl_type, "args");
            }
        } else {
            json_value func_type = json_get(func, "type");

            json_value func_type_tag = json_get(func_type, "_nodetype");
            if (func_type_tag.type == JSON_STRING &&
                strcmp(json_get_string(func_type_tag), "FuncDecl") == 0) {
                args = json_get(func_type, "args");
            }
        }

        // 파라미터 목록 추출
        if (args.type == JSON_OBJECT) {
            json_value params_val = json_get(args, "params");
            if (params_val.type == JSON_ARRAY) {
                get_parameters(params_val, params);
                has_params = 1;
            }
        } 
        
        /* IF 조건문 개수 추출 */
        int if_count = 0;
        if (nodetype_str && strcmp(nodetype_str, "FuncDef") == 0) {
            if_count = count_if_conditions(func);
        }

        /* 결과 출력 */
        printf("Funtion Name : %s\n", func_name);
        printf("Return Type : %s\n", return_type);
        printf("Parameter : %s\n", has_params ? params : "None");
        printf("IF Count: %d\n\n", if_count);
    }
}

/* 메모리 해제 */
json_free(json);      // JSON 파싱된 구조 해제
free(file_buff);      // 파일 읽기용 버퍼 해제

return 0;
}