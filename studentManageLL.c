#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include<stdlib.h>

typedef struct Student {
    char name[7];
    int birth;
    int phone;
    char address[30];
    char mother[10];
    char father[10];
    struct Student* next;
}sdt;

//add profile
sdt* createNode() {
    sdt* temp = (sdt*)malloc(sizeof(sdt));
    temp->next = NULL;
    printf("\n이름 : ");
    scanf("%s", temp->name);
    printf("\n생년월일 : ");
    scanf("%d", &temp->birth);
    printf("\n전화번호 : ");
    scanf("%d", &temp->phone);
    printf("\n주소 : ");
    scanf("%s", temp->address);
    printf("\n모 : ");
    scanf("%s", temp->mother);
    printf("\n부 : ");
    scanf("%s", temp->father);

    return temp;

}
void addProfile(sdt* head) {
    sdt* newprofile = createNode();
    if (head->next == NULL) {
        head->next = newprofile;
    }
    else {
        sdt* curr = head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = newprofile;
    }
}

//save profiles
void saveProfile(sdt* head) {
    FILE* file = fopen("data.data", "w");  
    if (file == NULL) {
        printf("파일을 열 수 없습니다.\n");
        return;
    }
    sdt* curr = head->next;
    while (curr != NULL) {
        fprintf(file, "%s %d %d %s %s %s\n", curr->name, curr->birth, curr->phone, curr->address, curr->mother, curr->father); //다음 예제는 랜섬웨어라는 떡밥
        curr = curr->next;
    }
    fclose(file);
    printf("프로필이 성공적으로 저장되었습니다.\n");
}

//load data file
void loadProfile(sdt* head) {
    FILE* file = fopen("data.data", "r");
    if (file == NULL) {
        printf("파일을 열 수 없습니다.\n");
        return;
    }
    sdt* temp;
    while (1) {
        temp = (sdt*)malloc(sizeof(sdt));
        if (fscanf(file, "%s %d %d %s %s %s", temp->name, &temp->birth, &temp->phone, temp->address, temp->mother, temp->father) != 6) {
            free(temp);
            break;
        }
        temp->next = NULL;
        if (head->next == NULL) {
            head->next = temp;
        }
        else {
            sdt* curr = head;
            while (curr->next != NULL) {
                curr = curr->next;
            }
            curr->next = temp;
        }
    }
    fclose(file);
}

//search profile
void searchProfile(sdt* head) {
    char name[7];
    int cnt = 0;
    printf("\n대상학생의 이름을 입력해주세요. : ");
    scanf("%s", &name);
    sdt* curr = head;
    if (head->next == NULL) { printf("\n학생 프로필이 존재하지 않습니다.\n"); return -1; }
    while (curr->next != NULL) {
        curr = curr->next;
        if (strcmp(curr->name, name) == 0) {
            cnt++;
            printf("\n이름 : %s", curr->name);
            printf("\n생년월일 : %d", curr->birth);
            printf("\n전화번호 : %d", curr->phone);
            printf("\n주소 : %s", curr->address);
            printf("\n모 : %s", curr->mother);
            printf("\n부 : %s\n", curr->father);
        }
    }
    printf("\n%d명의 학생을 찾았습니다.", cnt);
}

//print profiles
void printProfile(sdt* head) {
    if (head->next == NULL) { printf("\n학생 프로필이 없습니다.\n"); return -1; }
    sdt* temp = head->next;
    while (temp != NULL) {
        printf("\n이름 : %s", temp->name);
        printf("\n생년월일 : %d", temp->birth);
        printf("\n전화번호 : %d", temp->phone);
        printf("\n주소 : %s", temp->address);
        printf("\n모 : %s", temp->mother);
        printf("\n부 : %s\n", temp->father);
        temp = temp->next;
    }
    printf("NULL\n");
}

//delete profile (제일 복잡했음, 그래도 이지
void deleteProfile(sdt* head) {
    
    sdt* temp = (sdt*)malloc(sizeof(sdt));
    temp->next = NULL;
    printf("\n---------------------------------\n삭제할 프로필의 정보를 입력해주세요.\n---------------------------------");
    printf("\n이름 : ");
    scanf("%s", temp->name);    
    printf("\n생년월일 : ");
    scanf("%d", &temp->birth);
    printf("\n전화번호 : ");
    scanf("%d", &temp->phone);
    printf("\n주소 : ");
    scanf("%s", temp->address);    
    printf("\n모 : ");
    scanf("%s", temp->mother);  
    printf("\n부 : ");
    scanf("%s", temp->father);  

    sdt* curr = head->next;
    sdt* pre = head;
    if (head->next == NULL) { printf("\n학생 프로필이 존재하지 않습니다.\n"); return; }
    while (curr != NULL) {
        if (strcmp(curr->name, temp->name) == 0 &&
            curr->birth == temp->birth &&
            curr->phone == temp->phone &&
            strcmp(curr->address, temp->address) == 0 &&
            strcmp(curr->father, temp->father) == 0 &&
            strcmp(curr->mother, temp->mother) == 0) {
            pre->next = curr->next;
            free(curr);
            printf("성공적으로 삭제되었습니다.");
            return;
        }
        pre = curr;
        curr = curr->next;
    }
    printf("해당 프로필은 존재하지 않습니다.");
    free(temp);
}

//main function
int main() {
    int num;
    sdt* head = (sdt*)malloc(sizeof(sdt));
    head->next = NULL;
    loadProfile(head);
    while (1) {
        
        printf("\n---------------------------------\n     학생프로필 리스트 : 1\n      학생프로필 검색 : 2\n      학생프로필 삭제 : 3\n      학생프로필 추가 : 4\n---------------------------------\n");
        scanf("%d", &num);
        switch (num) {
        case 4:
            addProfile(head);
            saveProfile(head);
            break;
        case 1:
            printProfile(head);
            break;
        case 2:
            searchProfile(head);
            break;
        case 3:
            deleteProfile(head);
            saveProfile(head);
            break;

        }
    }

}
