using System;
using System.Collections;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Threading.Tasks;
using System.Text;
using UnityEngine;

public class NetworkManager : MonoBehaviour
{
    private TcpClient m_client;
    private NetworkStream m_stream;
    public GameObject VioletaPanel;

    public static NetworkManager m_instance;

    const string m_serverip = "127.0.0.1";
    const int m_server_port = 50000;

    void Awake()
    {
        if(m_instance == null)
        {
            m_instance = this;
        }
    }

    void Start()
    {
        StartCoroutine(ConnectToServer());
    }

    void OnApplicationQuit()
    {
        DisconnectFromServer();
    }

    private IEnumerator ConnectToServer()
    {
        m_client = new TcpClient();
        Task connectTask = Task.Run(() => ConnectToServerAsync());

        // 비동기적으로 연결이 완료될 때까지 대기
        yield return new WaitUntil(() => connectTask.IsCompleted);

        // 연결이 완료되었는지 확인
        if (connectTask.IsFaulted)
        {
            Debug.LogError("Connection Error: " + connectTask.Exception?.GetBaseException().Message);
            yield break;
        }

        // 서버로부터 응답 받기
        yield return StartCoroutine(ReceiveMessageFromServer());
    }

    private async Task ConnectToServerAsync()
    {
        try
        {
            await m_client.ConnectAsync(m_serverip, m_server_port); // 서버의 IP 주소와 포트 번호
            m_stream = m_client.GetStream();
            Debug.Log("Connected to server");
        }
        catch (Exception ex)
        {
            Debug.LogError("Connection Error: " + ex.Message);
        }
    }

    private void DisconnectFromServer()
    {
        if (m_stream != null)
        {
            m_stream.Close();
        }
        if (m_client != null)
        {
            m_client.Close();
        }
    }

    public void SendMessageToServer(string message)
    {
        byte[] buffer = Encoding.UTF8.GetBytes(message);
        Debug.Log("Message sent: " + message);
        m_stream.Write(buffer, 0, buffer.Length);
    }

    private IEnumerator ReceiveMessageFromServer()
    {
        byte[] buffer = new byte[1024];
        while (true)
        {
            if (m_stream.DataAvailable)
            {
                int bytesRead = m_stream.Read(buffer, 0, buffer.Length);
                if (bytesRead > 0)
                {
                    string responseMessage = Encoding.UTF8.GetString(buffer, 0, bytesRead);

                    DoVioleta(int.Parse(responseMessage));

                    Debug.Log("Received from server: " + responseMessage);
                    break; // 응답을 받은 후 종료
                }
            }

            // 서버의 응답을 기다리는 동안 잠시 대기
            yield return null;
        }
    }

    private void DoVioleta(int var_)
    {
        VioletaPanel.SetActive(true);

        Test test = VioletaPanel.GetComponentInChildren<Test>();

        test.OnRecvVioleta(var_, 0.5f);
    }
}