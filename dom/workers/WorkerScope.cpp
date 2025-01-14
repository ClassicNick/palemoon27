/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerScope.h"

#include "jsapi.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/DedicatedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/SharedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#include "Location.h"
#include "Navigator.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "Performance.h"
#include "ServiceWorkerClients.h"

#define UNWRAP_WORKER_OBJECT(Interface, obj, value)                           \
  UnwrapObject<prototypes::id::Interface##_workers,                           \
    mozilla::dom::Interface##Binding_workers::NativeType>(obj, value)

using namespace mozilla;
using namespace mozilla::dom;
USING_WORKERS_NAMESPACE

using mozilla::dom::cache::CacheStorage;
using mozilla::dom::indexedDB::IDBFactory;
using mozilla::ipc::PrincipalInfo;

BEGIN_WORKERS_NAMESPACE

WorkerGlobalScope::WorkerGlobalScope(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

WorkerGlobalScope::~WorkerGlobalScope()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerGlobalScope)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerGlobalScope,
                                                  DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerGlobalScope,
                                                DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerGlobalScope,
                                               DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();

  tmp->mWorkerPrivate->TraceTimeouts(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(WorkerGlobalScope, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WorkerGlobalScope, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerGlobalScope)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject*
WorkerGlobalScope::WrapObject(JSContext* aCx)
{
  MOZ_CRASH("We should never get here!");
}

Console*
WorkerGlobalScope::GetConsole()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mConsole) {
    mConsole = new Console(nullptr);
  }

  return mConsole;
}

already_AddRefed<CacheStorage>
WorkerGlobalScope::GetCaches(ErrorResult& aRv)
{
  if (!mCacheStorage) {
    MOZ_ASSERT(mWorkerPrivate);
    mCacheStorage = CacheStorage::CreateOnWorker(cache::DEFAULT_NAMESPACE, this,
                                                 mWorkerPrivate, aRv);
  }

  nsRefPtr<CacheStorage> ref = mCacheStorage;
  return ref.forget();
}

already_AddRefed<WorkerLocation>
WorkerGlobalScope::Location()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mLocation) {
    WorkerPrivate::LocationInfo& info = mWorkerPrivate->GetLocationInfo();

    mLocation = WorkerLocation::Create(info);
    MOZ_ASSERT(mLocation);
  }

  nsRefPtr<WorkerLocation> location = mLocation;
  return location.forget();
}

already_AddRefed<WorkerNavigator>
WorkerGlobalScope::Navigator()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mNavigator) {
    mNavigator = WorkerNavigator::Create(mWorkerPrivate->OnLine());
    MOZ_ASSERT(mNavigator);
  }

  nsRefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

already_AddRefed<WorkerNavigator>
WorkerGlobalScope::GetExistingNavigator() const
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

void
WorkerGlobalScope::Close(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  mWorkerPrivate->CloseInternal(aCx);
}

OnErrorEventHandlerNonNull*
WorkerGlobalScope::GetOnerror()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  EventListenerManager* elm = GetExistingListenerManager();
  return elm ? elm->GetOnErrorEventHandler() : nullptr;
}

void
WorkerGlobalScope::SetOnerror(OnErrorEventHandlerNonNull* aHandler)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  EventListenerManager* elm = GetOrCreateListenerManager();
  if (elm) {
    elm->SetEventHandler(aHandler);
  }
}

void
WorkerGlobalScope::ImportScripts(JSContext* aCx,
                                 const Sequence<nsString>& aScriptURLs,
                                 ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  scriptloader::Load(aCx, mWorkerPrivate, aScriptURLs, aRv);
}

int32_t
WorkerGlobalScope::SetTimeout(JSContext* aCx,
                              Function& aHandler,
                              const int32_t aTimeout,
                              const Sequence<JS::Value>& aArguments,
                              ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWorkerPrivate->SetTimeout(aCx, &aHandler, EmptyString(), aTimeout,
                                    aArguments, false, aRv);
}

int32_t
WorkerGlobalScope::SetTimeout(JSContext* /* unused */,
                              const nsAString& aHandler,
                              const int32_t aTimeout,
                              const Sequence<JS::Value>& /* unused */,
                              ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  Sequence<JS::Value> dummy;
  return mWorkerPrivate->SetTimeout(GetCurrentThreadJSContext(), nullptr,
                                    aHandler, aTimeout, dummy, false, aRv);
}

void
WorkerGlobalScope::ClearTimeout(int32_t aHandle, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->ClearTimeout(aHandle);
}

int32_t
WorkerGlobalScope::SetInterval(JSContext* aCx,
                               Function& aHandler,
                               const Optional<int32_t>& aTimeout,
                               const Sequence<JS::Value>& aArguments,
                               ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  bool isInterval = aTimeout.WasPassed();
  int32_t timeout = aTimeout.WasPassed() ? aTimeout.Value() : 0;

  return mWorkerPrivate->SetTimeout(aCx, &aHandler, EmptyString(), timeout,
                                    aArguments, isInterval, aRv);
}

int32_t
WorkerGlobalScope::SetInterval(JSContext* /* unused */,
                               const nsAString& aHandler,
                               const Optional<int32_t>& aTimeout,
                               const Sequence<JS::Value>& /* unused */,
                               ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  Sequence<JS::Value> dummy;

  bool isInterval = aTimeout.WasPassed();
  int32_t timeout = aTimeout.WasPassed() ? aTimeout.Value() : 0;

  return mWorkerPrivate->SetTimeout(GetCurrentThreadJSContext(), nullptr,
                                    aHandler, timeout, dummy, isInterval, aRv);
}

void
WorkerGlobalScope::ClearInterval(int32_t aHandle, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->ClearTimeout(aHandle);
}

void
WorkerGlobalScope::Atob(const nsAString& aAtob, nsAString& aOutput, ErrorResult& aRv) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Atob(aAtob, aOutput);
}

void
WorkerGlobalScope::Btoa(const nsAString& aBtoa, nsAString& aOutput, ErrorResult& aRv) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Btoa(aBtoa, aOutput);
}

void
WorkerGlobalScope::Dump(const Optional<nsAString>& aString) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!aString.WasPassed()) {
    return;
  }

  if (!mWorkerPrivate->DumpEnabled()) {
    return;
  }

  NS_ConvertUTF16toUTF8 str(aString.Value());

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Goanna", "%s", str.get());
#endif
  fputs(str.get(), stdout);
  fflush(stdout);
}

Performance*
WorkerGlobalScope::GetPerformance()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mPerformance) {
    mPerformance = new Performance(mWorkerPrivate);
  }

  return mPerformance;
}

already_AddRefed<Promise>
WorkerGlobalScope::Fetch(const RequestOrUSVString& aInput,
                         const RequestInit& aInit, ErrorResult& aRv)
{
  return FetchRequest(this, aInput, aInit, aRv);
}

already_AddRefed<IDBFactory>
WorkerGlobalScope::GetIndexedDB(ErrorResult& aErrorResult)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<IDBFactory> indexedDB = mIndexedDB;

  if (!indexedDB) {
    if (!mWorkerPrivate->IsIndexedDBAllowed()) {
      NS_WARNING("IndexedDB is not allowed in this worker!");
      return nullptr;
    }

    JSContext* cx = mWorkerPrivate->GetJSContext();
    MOZ_ASSERT(cx);

    JS::Rooted<JSObject*> owningObject(cx, GetGlobalJSObject());
    MOZ_ASSERT(owningObject);

    const PrincipalInfo& principalInfo = mWorkerPrivate->GetPrincipalInfo();

    nsresult rv =
      IDBFactory::CreateForWorker(cx,
                                  owningObject,
                                  principalInfo,
                                  mWorkerPrivate->WindowID(),
                                  getter_AddRefs(indexedDB));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aErrorResult = rv;
      return nullptr;
    }

    mIndexedDB = indexedDB;
  }

  return indexedDB.forget();
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate)
: WorkerGlobalScope(aWorkerPrivate)
{
}

bool
DedicatedWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                             JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mWorkerPrivate->IsSharedWorker());

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return DedicatedWorkerGlobalScopeBinding_workers::Wrap(aCx, this, this,
                                                         options,
                                                         GetWorkerPrincipal(),
                                                         true, aReflector);
}

void
DedicatedWorkerGlobalScope::PostMessage(JSContext* aCx,
                                        JS::Handle<JS::Value> aMessage,
                                        const Optional<Sequence<JS::Value>>& aTransferable,
                                        ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->PostMessageToParent(aCx, aMessage, aTransferable, aRv);
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                 const nsCString& aName)
: WorkerGlobalScope(aWorkerPrivate), mName(aName)
{
}

bool
SharedWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsSharedWorker());

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return SharedWorkerGlobalScopeBinding_workers::Wrap(aCx, this, this, options,
                                                      GetWorkerPrincipal(),
                                                      true, aReflector);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope,
                                   mClients)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkerGlobalScope)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                   const nsACString& aScope)
  : WorkerGlobalScope(aWorkerPrivate),
    mScope(NS_ConvertUTF8toUTF16(aScope))
{
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope()
{
}

bool
ServiceWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                           JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return ServiceWorkerGlobalScopeBinding_workers::Wrap(aCx, this, this, options,
                                                       GetWorkerPrincipal(),
                                                       true, aReflector);
}

ServiceWorkerClients*
ServiceWorkerGlobalScope::Clients()
{
  if (!mClients) {
    mClients = new ServiceWorkerClients(this);
  }

  return mClients;
}

bool
GetterOnlyJSNative(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS_ReportErrorNumber(aCx, js::GetErrorMessage, nullptr, JSMSG_GETTER_ONLY);
  return false;
}

namespace {

class WorkerScopeUnregisterRunnable;
class UnregisterResultRunnable final : public WorkerRunnable
{
public:
  enum State { Succeeded, Failed };

  UnregisterResultRunnable(WorkerPrivate* aWorkerPrivate,
                           WorkerScopeUnregisterRunnable* aRunnable,
                           State aState, bool aValue)
    : WorkerRunnable(aWorkerPrivate,
                     WorkerThreadUnchangedBusyCount)
    , mRunnable(aRunnable), mState(aState), mValue(aValue)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mRunnable);
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

private:
  nsRefPtr<WorkerScopeUnregisterRunnable> mRunnable;
  State mState;
  bool mValue;
};

class WorkerScopeUnregisterRunnable final : public nsRunnable
                                              , public nsIServiceWorkerUnregisterCallback
                                              , public WorkerFeature
{
  WorkerPrivate* mWorkerPrivate;
  nsString mScope;

  // Worker thread only.
  nsRefPtr<Promise> mWorkerPromise;
  bool mCleanedUp;

  ~WorkerScopeUnregisterRunnable()
  {
    MOZ_ASSERT(mCleanedUp);
  }

public:
  NS_DECL_ISUPPORTS_INHERITED

  WorkerScopeUnregisterRunnable(WorkerPrivate* aWorkerPrivate,
                                Promise* aWorkerPromise,
                                const nsAString& aScope)
    : mWorkerPrivate(aWorkerPrivate)
    , mScope(aScope)
    , mWorkerPromise(aWorkerPromise)
    , mCleanedUp(false)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPromise);

    if (!mWorkerPrivate->AddFeature(mWorkerPrivate->GetJSContext(), this)) {
      MOZ_ASSERT(false, "cannot add the worker feature!");
      mWorkerPromise = nullptr;
      mCleanedUp = true;
      return;
    }
  }

  Promise*
  WorkerPromise() const
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!mCleanedUp);
    return mWorkerPromise;
  }

  NS_IMETHODIMP
  UnregisterSucceeded(bool aState) override
  {
    AssertIsOnMainThread();

    nsRefPtr<UnregisterResultRunnable> runnable =
      new UnregisterResultRunnable(mWorkerPrivate, this,
                                   UnregisterResultRunnable::Succeeded, aState);
    runnable->Dispatch(nullptr);
    return NS_OK;
  }

  NS_IMETHODIMP
  UnregisterFailed() override
  {
    AssertIsOnMainThread();

    nsRefPtr<UnregisterResultRunnable> runnable =
      new UnregisterResultRunnable(mWorkerPrivate, this,
                                   UnregisterResultRunnable::Failed, false);
    runnable->Dispatch(nullptr);
    return NS_OK;
  }

  void
  CleanUp(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (mCleanedUp) {
      return;
    }

    mWorkerPrivate->RemoveFeature(aCx, this);
    mWorkerPromise = nullptr;
    mCleanedUp = true;
  }

  NS_IMETHODIMP
  Run() override
  {
    AssertIsOnMainThread();

    nsresult rv;
    nsCOMPtr<nsIServiceWorkerManager> swm =
      do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      UnregisterFailed();
      return NS_OK;
    }

    // We don't need to check if the principal can load this mScope because a
    // ServiceWorkerGlobalScope can always unregister itself.

    rv = swm->Unregister(mWorkerPrivate->GetPrincipal(), this, mScope);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      UnregisterFailed();
      return NS_OK;
    }

    return NS_OK;
  }

  virtual bool Notify(JSContext* aCx, workers::Status aStatus) override
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aStatus > workers::Running);
    CleanUp(aCx);
    return true;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(WorkerScopeUnregisterRunnable, nsRunnable,
                            nsIServiceWorkerUnregisterCallback)

bool
UnregisterResultRunnable::WorkerRun(JSContext* aCx,
                                    WorkerPrivate* aWorkerPrivate)
{
  if (mState == Failed) {
    mRunnable->WorkerPromise()->MaybeReject(aCx, JS::UndefinedHandleValue);
  } else {
    mRunnable->WorkerPromise()->MaybeResolve(mValue);
  }

  mRunnable->CleanUp(aCx);
  return true;
}

} // anonymous namespace

already_AddRefed<Promise>
ServiceWorkerGlobalScope::Unregister(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  nsRefPtr<Promise> promise = Promise::Create(this, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<WorkerScopeUnregisterRunnable> runnable =
    new WorkerScopeUnregisterRunnable(mWorkerPrivate, promise, mScope);

  // Ensure the AddFeature succeeded before dispatching.
  // Otherwise we let the promise remain pending since script is going to stop
  // soon anyway.
  if (runnable->WorkerPromise()) {
    NS_DispatchToMainThread(runnable);
  }

  return promise.forget();
}

namespace {

class UpdateRunnable final : public nsRunnable
{
  nsString mScope;

public:
  explicit UpdateRunnable(const nsAString& aScope)
    : mScope(aScope)
  { }

  NS_IMETHODIMP
  Run()
  {
    AssertIsOnMainThread();

    nsresult rv;
    nsCOMPtr<nsIServiceWorkerManager> swm =
      do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    swm->Update(mScope);
    return NS_OK;
  }
};

} //anonymous namespace

void
ServiceWorkerGlobalScope::Update()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  nsRefPtr<UpdateRunnable> runnable =
    new UpdateRunnable(mScope);
  NS_DispatchToMainThread(runnable);
}

END_WORKERS_NAMESPACE
